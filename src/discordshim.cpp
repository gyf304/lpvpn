#include <discord_game_sdk.h>
#include <MemoryModule.h>

#include "rc.h"

typedef enum EDiscordResult (*DiscordCreateFunc)(DiscordVersion version, struct DiscordCreateParams* params, struct IDiscordCore** result);

enum EDiscordResult DiscordCreate(DiscordVersion version, struct DiscordCreateParams* params, struct IDiscordCore** result) {
    auto file = lpvpn::fs.open("discord_game_sdk.dll");
    auto module = MemoryLoadLibrary(file.begin());
    DiscordCreateFunc func = (DiscordCreateFunc)MemoryGetProcAddress(module, "DiscordCreate");
    return func(version, params, result);
}
