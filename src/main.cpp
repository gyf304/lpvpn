// wxWidgets "Hello world" Program
// For compilers that support precompilation, includes "wx/wx.h".

#include <wx/wx.h>

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(lpvpn);

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

class HostJoinSelectPanel : public wxPanel
{
public:
	HostJoinSelectPanel(wxWindow *parent);
	wxButton *advancedSettingsButton;
	wxTextCtrl *hostIpCidrInput;
	int expandedH;
};

class NetworkPanel : public wxPanel
{
public:
	NetworkPanel(wxWindow *parent, bool host);
	wxStaticText *ipLabel;
	wxStaticText *sentBytesLabel;
	wxStaticText *receivedBytesLabel;
};

class MainFrame : public wxFrame
{
public:
	MainFrame();
	HostJoinSelectPanel *hostJoinSelectPanel;
	NetworkPanel *hostPanel;

private:
	void OnHost(wxCommandEvent &event);
	void OnJoin(wxCommandEvent &event);
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
	ID_SYNC_TIMER,
	ID_TOGGLE_ADVANCED
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_BUTTON(ID_HOST, MainFrame::OnHost)
	EVT_BUTTON(ID_JOIN, MainFrame::OnJoin)
	EVT_BUTTON(ID_INVITE, MainFrame::OnInvite)
	EVT_TIMER(ID_SYNC_TIMER, MainFrame::OnTimer)
	EVT_BUTTON(ID_TOGGLE_ADVANCED, MainFrame::OnToggleAdvanced)
	wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(App);

auto fs = cmrc::lpvpn::get_filesystem();

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
	if (discordnet == nullptr)
	{
		return;
	}
	auto exception = discordnet->getException();
	if (exception.has_value())
	{
		wxMessageBox(exception.value().what(), "Error", wxOK | wxICON_ERROR);
		this->Close();
		return;
	}
	auto message = discordnet->getMessage();
	if (message.has_value())
	{
		wxMessageBox(message.value(), "Message", wxOK | wxICON_INFORMATION);
	}
	auto ip = discordnet->getAddress();
	auto receivedBytes = discordnet->getReceivedBytes();
	auto sentBytes = discordnet->getSentBytes();
	if (ip.has_value())
	{
		hostPanel->ipLabel->SetLabelText("IP: " + cidr::format(ip.value()));
	}
	hostPanel->sentBytesLabel->SetLabelText("Sent Bytes: " + std::to_string(sentBytes));
	hostPanel->receivedBytesLabel->SetLabelText("Received Bytes: " + std::to_string(receivedBytes));
}

MainFrame::MainFrame()
	: wxFrame(NULL, wxID_ANY, "LP VPN", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX))
{
	// this->SetIcon();
	auto iconPng = fs.open("icon.png");
	auto iconBitmap = wxBitmap::NewFromPNGData(iconPng.begin(), iconPng.size()).ConvertToImage().Scale(64, 64);
	auto icon = wxIcon();
	icon.CopyFromBitmap(iconBitmap);
	this->SetIcon(icon);

	hostJoinSelectPanel = new HostJoinSelectPanel(this);
	this->SetClientSize(hostJoinSelectPanel->GetSize());
	this->CenterOnScreen();
}

void MainFrame::OnToggleAdvanced(wxCommandEvent &event)
{
	hostJoinSelectPanel->advancedSettingsButton->Destroy();
	auto size = hostJoinSelectPanel->GetSize();
	size.SetHeight(hostJoinSelectPanel->expandedH);
	hostJoinSelectPanel->SetSize(size);
	this->SetClientSize(hostJoinSelectPanel->GetSize());
}

void MainFrame::OnHost(wxCommandEvent &event)
{
	try
	{
		auto cidrStr = std::string(hostJoinSelectPanel->hostIpCidrInput->GetLineText(0));
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
		discordnet = std::make_unique<lpvpn::discordnet::DiscordNet>(hostCidr);
		hostJoinSelectPanel->Destroy();
		hostPanel = new NetworkPanel(this, true);
		this->SetClientSize(hostPanel->GetSize());
		(new wxTimer(this, ID_SYNC_TIMER))->Start(1000);
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
		discordnet = std::make_unique<lpvpn::discordnet::DiscordNet>();
		hostJoinSelectPanel->Destroy();
		hostPanel = new NetworkPanel(this, false);
		this->SetClientSize(hostPanel->GetSize());
		(new wxTimer(this, ID_SYNC_TIMER))->Start(1000);
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

HostJoinSelectPanel::HostJoinSelectPanel(wxWindow *parent)
	: wxPanel(parent)
{
	int w = 240, h = 140, padding = 8;
	int buttonW = 80, buttonH = 80;

	SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	SetForegroundColour(wxColour(0xff, 0xff, 0xff));

	this->SetPosition(wxPoint(0, 0));
	this->SetClientSize(wxSize(w, h));

	auto hostButtonDefaultPng = fs.open("btn-host-default.png");
	auto hostButtonDefaultImage = wxBitmap::NewFromPNGData(hostButtonDefaultPng.begin(), hostButtonDefaultPng.size()).ConvertToImage().Scale(64, 64);
	auto hostButtonHoverPng = fs.open("btn-host-hover.png");
	auto hostButtonHoverImage = wxBitmap::NewFromPNGData(hostButtonHoverPng.begin(), hostButtonHoverPng.size()).ConvertToImage().Scale(64, 64);
	wxButton *hostButton = new wxButton(this, ID_HOST, "Host VPN", wxPoint(w / 4 - buttonW / 2, h / 2 - buttonH / 2), wxSize(buttonW, buttonH), wxBORDER_NONE);
	hostButton->SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	hostButton->SetForegroundColour(wxColour(0xff, 0xff, 0xff));
	hostButton->SetBitmap(hostButtonDefaultImage, wxTOP);
	hostButton->SetBitmapHover(hostButtonHoverImage);

	auto joinButtonDefaultPng = fs.open("btn-join-default.png");
	auto joinButtonDefaultImage = wxBitmap::NewFromPNGData(joinButtonDefaultPng.begin(), joinButtonDefaultPng.size()).ConvertToImage().Scale(64, 64);
	auto joinButtonHoverPng = fs.open("btn-join-hover.png");
	auto joinButtonHoverImage = wxBitmap::NewFromPNGData(joinButtonHoverPng.begin(), joinButtonHoverPng.size()).ConvertToImage().Scale(64, 64);
	wxButton *joinButton = new wxButton(this, ID_JOIN, "Join VPN", wxPoint(w / 4 * 3 - buttonW / 2, h / 2 - buttonH / 2), wxSize(buttonW, buttonH), wxBORDER_NONE);
	joinButton->SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	joinButton->SetForegroundColour(wxColour(0xff, 0xff, 0xff));
	joinButton->SetBitmap(joinButtonDefaultImage, wxTOP);
	joinButton->SetBitmapHover(joinButtonHoverImage);

	advancedSettingsButton = new wxButton(this, ID_TOGGLE_ADVANCED, "Show Advanced Settings", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
	advancedSettingsButton->SetBackgroundColour(wxColour(0x44, 0x44, 0x44));
	advancedSettingsButton->SetForegroundColour(wxColour(0xaa, 0xaa, 0xaa));
	auto advancedSettingsButtonSize = advancedSettingsButton->GetSize();
	auto advancedSettingsButtonPosition = advancedSettingsButton->GetPosition();
	advancedSettingsButtonSize.SetWidth(w);
	advancedSettingsButtonPosition.y = h - advancedSettingsButtonSize.GetHeight() - 4;
	advancedSettingsButton->SetSize(advancedSettingsButtonSize);
	advancedSettingsButton->SetPosition(advancedSettingsButtonPosition);

	wxWindow *prev = nullptr;

	auto hostIpCidrText = new wxStaticText(this, wxID_ANY, "Host IP CIDR", wxPoint(padding, h + padding));
	auto hostIpCidrTextSize = hostIpCidrText->GetSize();
	hostIpCidrTextSize.SetWidth(w - padding * 2);
	hostIpCidrText->SetSize(hostIpCidrTextSize);
	prev = hostIpCidrText;

	hostIpCidrInput = new wxTextCtrl(this, wxID_ANY, "192.168.42.1/24", wxPoint(padding, nextY(prev, padding)));
	auto hostIpCidrInputSize = hostIpCidrInput->GetSize();
	hostIpCidrInputSize.SetWidth(w - padding * 2);
	hostIpCidrInput->SetSize(hostIpCidrInputSize);
	prev = hostIpCidrInput;

	expandedH = nextY(prev, padding);
}

NetworkPanel::NetworkPanel(wxWindow *parent, bool host) : wxPanel(parent)
{
	int w = 240;

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

	if (host)
	{
		auto inviteButton = new wxButton(this, ID_INVITE, "Invite", wxPoint(8, nextY(prev, 16)));
		auto inviteButtonSize = inviteButton->GetSize();
		inviteButtonSize.SetWidth(w - 16);
		inviteButton->SetSize(inviteButtonSize);
		prev = inviteButton;
	}

	this->SetClientSize(wxSize(w, nextY(prev, 8)));
}
