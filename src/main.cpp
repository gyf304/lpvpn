// wxWidgets "Hello world" Program
// For compilers that support precompilation, includes "wx/wx.h".

#include <wx/wx.h>

#include "rc.h"
#include "guidparse.h"
#include "adapter.h"
#include "discordnet.h"

static std::unique_ptr<lpvpn::discordnet::DiscordNet> discordnet = nullptr;

static int nextY(wxWindow *w, int inc)
{
	if (w == nullptr)
	{
		return inc;
	}
	return w->GetPosition().y + w->GetSize().GetHeight() + inc;
}


class NetworkPanel : public wxPanel
{
public:
	NetworkPanel(wxWindow *parent);
	wxStaticText *ipLabel;
	wxStaticText *sentBytesLabel;
	wxStaticText *receivedBytesLabel;
	wxButton* advancedSettingsButton;
	wxTextCtrl* hostIpCidrInput;
	wxButton* inviteButton;
	wxButton* hostButton;
	wxButton* leaveButton;
	int expandedH;
};

class MainFrame : public wxFrame
{
public:
	MainFrame();
	NetworkPanel *hostPanel;

private:
	void OnHost(wxCommandEvent &event);
	void OnJoin(wxCommandEvent &event);
	void OnLeave(wxCommandEvent& event);
	void OnToggleAdvanced(wxCommandEvent &event);
	void OnInvite(wxCommandEvent &event);
	void OnTimer(wxTimerEvent &event);

	wxDECLARE_EVENT_TABLE();
};

class App : public wxApp
{
private:
	MainFrame *mainFrame;

public:
	bool OnInit();
};

enum
{
	ID_HOST = 1,
	ID_JOIN,
	ID_INVITE,
	ID_LEAVE,
	ID_SYNC_TIMER,
	ID_TOGGLE_ADVANCED
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_BUTTON(ID_HOST, MainFrame::OnHost)
	EVT_BUTTON(ID_JOIN, MainFrame::OnJoin)
	EVT_BUTTON(ID_INVITE, MainFrame::OnInvite)
	EVT_BUTTON(ID_LEAVE, MainFrame::OnLeave)
	EVT_TIMER(ID_SYNC_TIMER, MainFrame::OnTimer)
	EVT_BUTTON(ID_TOGGLE_ADVANCED, MainFrame::OnToggleAdvanced)
	wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(App);

bool App::OnInit()
{
#if defined(_WIN32)
	BOOL isWow64 = false;
	IsWow64Process(GetCurrentProcess(), &isWow64);
	if (isWow64) {
		wxMessageBox("You must run the 64 bit version on a 64 bit OS.", "Error", wxOK | wxICON_ERROR);
		return false;
	}
#endif
	wxImage::AddHandler(new wxPNGHandler());
	MainFrame *frame = new MainFrame();
	frame->Show(true);
	return true;
}

void MainFrame::OnTimer(wxTimerEvent &event)
{
	hostPanel->hostButton->Disable();
	hostPanel->inviteButton->Disable();
	hostPanel->leaveButton->Disable();
	if (discordnet == nullptr)
	{
		return;
	}
	auto [exception, message, ip] = [&]() {
		std::lock_guard lg(discordnet->mutex);
		auto result = std::make_tuple(std::move(discordnet->exception), std::move(discordnet->message), discordnet->address);
		discordnet->exception.reset();
		discordnet->message.reset();
		return result;
	}();
	auto conn = discordnet->connection.load();
	auto receivedBytes = discordnet->receivedBytes.load();
	auto sentBytes = discordnet->sentBytes.load();
	auto task = discordnet->task.load();
	if (exception.has_value()) {
		wxMessageBox(exception.value().what(), "Error", wxOK | wxICON_ERROR);
		this->Close();
		return;
	}
	if (message.has_value()) {
		wxMessageBox(message.value(), "Message", wxOK | wxICON_INFORMATION);
	}

	using Task = lpvpn::discordnet::DiscordNet::Task;
	using Connection = lpvpn::discordnet::DiscordNet::Connection;


	switch (conn) {
	case Connection::Disconnected:
		hostPanel->ipLabel->SetLabelText("Not connected");
		hostPanel->hostButton->Enable();
		break;
	case Connection::WaitingForIP:
		hostPanel->ipLabel->SetLabelText("Waiting for IP");
		hostPanel->leaveButton->Enable();
		break;
	case Connection::Connected:
		assert(ip.has_value());
		hostPanel->ipLabel->SetLabelText("Connected: " + cidr::format(ip.value()));
		hostPanel->leaveButton->Enable();
		break;
	case Connection::Hosting:
		assert(ip.has_value());
		hostPanel->ipLabel->SetLabelText("Hosting: " + cidr::format(ip.value()));
		hostPanel->inviteButton->Enable();
		hostPanel->leaveButton->Enable();
		break;
	}

	if (task != Task::None) {
		hostPanel->hostButton->Disable();
		hostPanel->inviteButton->Disable();
		hostPanel->leaveButton->Disable();
	}

	hostPanel->sentBytesLabel->SetLabelText("Sent Bytes: " + std::to_string(sentBytes));
	hostPanel->receivedBytesLabel->SetLabelText("Received Bytes: " + std::to_string(receivedBytes));
}

MainFrame::MainFrame()
	: wxFrame(NULL, wxID_ANY, "LP VPN", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxICON_NONE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX ))
{
	auto iconPng = lpvpn::fs.open("icon.png");
	auto iconBitmap = wxBitmap::NewFromPNGData(iconPng.begin(), iconPng.size()).ConvertToImage().Scale(64, 64);
	auto icon = wxIcon();
	icon.CopyFromBitmap(iconBitmap);
	this->SetIcon(icon);

	hostPanel = new NetworkPanel(this);
	this->SetClientSize(hostPanel->GetSize());
	this->CenterOnScreen();
	try
	{
		discordnet = std::make_unique<lpvpn::discordnet::DiscordNet>();
		(new wxTimer(this, ID_SYNC_TIMER))->Start(1000);
	}
	catch (std::exception& e)
	{
		wxMessageBox("Cannot start VPN: " + std::string(e.what()), "Cannot start VPN", wxOK | wxICON_ERROR);
		this->Close();
	}
}

void MainFrame::OnToggleAdvanced(wxCommandEvent &event)
{
	hostPanel->advancedSettingsButton->Destroy();
	auto size = hostPanel->GetSize();
	size.SetHeight(hostPanel->expandedH);
	hostPanel->SetSize(size);
	this->SetClientSize(hostPanel->GetSize());
}

void MainFrame::OnHost(wxCommandEvent &event)
{
	try
	{
		auto cidrStr = std::string(hostPanel->hostIpCidrInput->GetLineText(0));
		auto hostCidr = cidr::parse(cidrStr.c_str());
		if (hostCidr.maskBits != 24)
		{
			wxMessageBox("You must use a /24 subnet", "Cannot start VPN", wxOK | wxICON_ERROR);
			return;
		}
		if (((uint8_t *)&hostCidr.addr.s_addr)[3] != 1)
		{
			wxMessageBox("IP address must end in .1", "Cannot start VPN", wxOK | wxICON_ERROR);
			return;
		}
		discordnet->host(hostCidr);
		// TODO SIGNAL HOST
		this->SetClientSize(hostPanel->GetSize());
	}
	catch (std::exception &e)
	{
		wxMessageBox("Cannot start VPN: " + std::string(e.what()), "Cannot start VPN", wxOK | wxICON_ERROR);
		this->Close();
	}
}

void MainFrame::OnJoin(wxCommandEvent &event)
{
	try
	{
		this->SetClientSize(hostPanel->GetSize());
	}
	catch (std::exception &e)
	{
		wxMessageBox("Cannot start VPN: " + std::string(e.what()), "Cannot start VPN", wxOK | wxICON_ERROR);
		this->Close();
	}
	auto timer = new wxTimer(this, ID_SYNC_TIMER);
	timer->Start(1000);
}

void MainFrame::OnInvite(wxCommandEvent &event)
{
	discordnet->invite();
}
void MainFrame::OnLeave(wxCommandEvent& event)
{
	discordnet->leave();
}

NetworkPanel::NetworkPanel(wxWindow *parent) : wxPanel(parent)
{
	int w = 240;
	int padding = 8;

	SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	SetForegroundColour(wxColour(0xff, 0xff, 0xff));

	this->SetPosition(wxPoint(0, 0));

	wxWindow *prev = nullptr;

	ipLabel = new wxStaticText(this, wxID_ANY, "Waiting for connection", wxPoint(8, nextY(prev, 8)));
	auto ipLabelSize = ipLabel->GetSize();
	ipLabelSize.SetWidth(w - 16);
	ipLabel->SetSize(ipLabelSize);
	prev = ipLabel;

	sentBytesLabel = new wxStaticText(this, wxID_ANY, "", wxPoint(8, nextY(prev, 8)));
	auto sentBytesLabelSize = sentBytesLabel->GetSize();
	sentBytesLabelSize.SetWidth(w - 16);
	sentBytesLabel->SetSize(sentBytesLabelSize);
	prev = sentBytesLabel;

	receivedBytesLabel = new wxStaticText(this, wxID_ANY, "", wxPoint(8, nextY(prev, 4)));
	auto receivedBytesLabelSize = receivedBytesLabel->GetSize();
	receivedBytesLabelSize.SetWidth(w - 16);
	receivedBytesLabel->SetSize(receivedBytesLabelSize);
	prev = receivedBytesLabel;

	inviteButton = new wxButton(this, ID_INVITE, "Invite", wxPoint(8, nextY(prev, 16)));
	auto inviteButtonSize = inviteButton->GetSize();
	inviteButtonSize.SetWidth(w - 16);
	inviteButton->SetSize(inviteButtonSize);
	prev = inviteButton;

	hostButton = new wxButton(this, ID_HOST, "Host", wxPoint(8, nextY(prev, 16)));
	auto hostButtonSize = hostButton->GetSize();
	hostButtonSize.SetWidth(w - 16);
	hostButton->SetSize(inviteButtonSize);
	prev = hostButton;

	leaveButton = new wxButton(this, ID_LEAVE, "Leave", wxPoint(8, nextY(prev, 16)));
	auto leaveButtonSize = leaveButton->GetSize();
	leaveButtonSize.SetWidth(w - 16);
	leaveButton->SetSize(inviteButtonSize);
	prev = leaveButton;

	advancedSettingsButton = new wxButton(this, ID_TOGGLE_ADVANCED, "Show Advanced Settings", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
	advancedSettingsButton->SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	advancedSettingsButton->SetForegroundColour(wxColour(0xaa, 0xaa, 0xaa));
	auto advancedSettingsButtonSize = advancedSettingsButton->GetSize();
	auto advancedSettingsButtonPosition = advancedSettingsButton->GetPosition();
	advancedSettingsButtonSize.SetWidth(w);
	advancedSettingsButtonPosition.y = nextY(prev, 16);
	advancedSettingsButton->SetSize(advancedSettingsButtonSize);
	advancedSettingsButton->SetPosition(advancedSettingsButtonPosition);
	prev = advancedSettingsButton;

	this->SetClientSize(wxSize(w, nextY(prev, 8)));

	auto hostIpCidrText = new wxStaticText(this, wxID_ANY, "Host IP CIDR", wxPoint(padding, nextY(prev, padding)));
	auto hostIpCidrTextSize = hostIpCidrText->GetSize();
	hostIpCidrTextSize.SetWidth(w - padding * 2);
	hostIpCidrText->SetSize(hostIpCidrTextSize);
	prev = hostIpCidrText;

	hostIpCidrInput = new wxTextCtrl(this, wxID_ANY, "192.168.42.1/24", wxPoint(padding, nextY(prev, padding)));
	auto hostIpCidrInputSize = hostIpCidrInput->GetSize();
	hostIpCidrInputSize.SetWidth(w - padding * 2);
	hostIpCidrInput->SetSize(hostIpCidrInputSize);
	prev = hostIpCidrInput;


	hostButton->Disable();
	inviteButton->Disable();
	leaveButton->Disable();

	expandedH = nextY(prev, padding);
}
