#include "API/ARK/Ark.h"

namespace mlsk
{

// For functions that are not defined in the ArkServerApi headers
struct FUniqueNetIdRepl : ::FUniqueNetIdRepl
{
    void SetUniqueNetId(TSharedPtr<FUniqueNetId, 0>* InUniqueNetId) { NativeCall<void, TSharedPtr<FUniqueNetId, 0>*>(this, "FUniqueNetIdRepl.SetUniqueNetId", InUniqueNetId); }
    FString ToString() { FString result; return *NativeCall<FString*, FString*>(this, "FUniqueNetIdRepl.ToString", &result); }
};

}

DECLARE_HOOK(AShooterGameMode_PreLogin, void, AShooterGameMode*, FString*, FString*, TSharedPtr<FUniqueNetId, 0>*, FString*, FString*, UNetConnection*);
void Hook_AShooterGameMode_PreLogin(AShooterGameMode* _this, FString* Options, FString* Address, TSharedPtr<FUniqueNetId, 0>* UniqueId, FString* authToken, FString* ErrorMessage, UNetConnection* Connection)
{
    AShooterGameMode_PreLogin_original(_this, Options, Address, UniqueId, authToken, ErrorMessage, Connection);

    // If we didn't receive a "already connected error", return
    if (!ErrorMessage->Equals("There is already a player with this account connected!"))
        return;

    // Clear the error
    ErrorMessage->Empty();

    // If we received an "already connected" error, AShooterGameMode::PreLogin() returned without calling
    // AGameMode::PreLogin(). Call it here to complete the PreLogin process and validate the new connection.
    // Use NativeCall becuase the definition in the ArkServerApi headers is missing the "Connection" arg.
    NativeCall<void, FString*, FString*, TSharedPtr<FUniqueNetId, 0>*, FString*, FString*, UNetConnection*>(_this, "AGameMode.PreLogin", Options, Address, UniqueId, authToken, ErrorMessage, Connection);

    // If AGameMode::PreLogin() resulted in any errors, return
    if (!ErrorMessage->IsEmpty())
        return;

    // Get the new connection's UniqueNetId
    mlsk::FUniqueNetIdRepl newNetId;
    newNetId.SetUniqueNetId(UniqueId);
    FString newNetIdString = newNetId.ToString().ToLower();

    // Find the existing controller
    APlayerController* existingPC = nullptr;
    for (TAutoWeakObjectPtr<APlayerController> weakPC : _this->GetWorld()->PlayerControllerListField()) {
        if (!weakPC.Get())
            continue;

        if (!weakPC->PlayerStateField())
            continue;

        if (newNetIdString.Equals(static_cast<mlsk::FUniqueNetIdRepl>(weakPC->PlayerStateField()->UniqueIdField()).ToString().ToLower())) {
            existingPC = weakPC;
            break;
        }
    }
    if (!existingPC || !existingPC->PlayerField())
        return; // This shouldn't happen

    // Calling the existing KickPlayer functions here can cause the new connection to timeout. So we
    // implement a modified version of AGameSession::KickPlayer().

    // Destroy the existing PlayerController at the end of the tick
    existingPC->Destroy(false, true);

    // Clean up and close the existing NetConnection
    NativeCall<void>(static_cast<UNetConnection*>(existingPC->PlayerField()), "UNetConnection.CleanUp");
}

// Called by ArkServerApi when the plugin is loaded, do pre-"server ready" initialization here
extern "C" __declspec(dllexport) void Plugin_Init()
{
    Log::Get().Init(PROJECT_NAME);

    ArkApi::GetHooks().SetHook("AShooterGameMode.PreLogin", Hook_AShooterGameMode_PreLogin,
        &AShooterGameMode_PreLogin_original);
}

// Called by ArkServerApi when the plugin is unloaded, do cleanup here
extern "C" __declspec(dllexport) void Plugin_Unload()
{
    ArkApi::GetHooks().DisableHook("AShooterGameMode.PreLogin", Hook_AShooterGameMode_PreLogin);
}