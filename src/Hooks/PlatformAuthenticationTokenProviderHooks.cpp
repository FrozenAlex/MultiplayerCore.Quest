#include "hooking.hpp"
#include "logging.hpp"
#include "tasks.hpp"
#include "Hooks/NetworkConfigHooks.hpp"
#include "Hooks/LegacyAuthentication.hpp"
#include "GlobalNamespace/PlatformAuthenticationTokenProvider.hpp"
#include "GlobalNamespace/XPlatformAccessTokenData.hpp"
#include "GlobalNamespace/PlatformEnvironment.hpp"
#include "Oculus/Platform/Users.hpp"
#include "Oculus/Platform/Request_1.hpp"
#include "Oculus/Platform/Message_1.hpp"
#include "Oculus/Platform/Models/UserProof.hpp"
#include "custom-types/shared/delegate.hpp"
#include <mutex>
namespace MultiplayerCore::Hooks {
static std::mutex credentialsMutex;
static LegacyCredentials credentials;
LegacyCredentials GetLegacyCredentials() {
    std::lock_guard lock(credentialsMutex);
    return credentials;
}
}
using namespace MultiplayerCore;
using namespace MultiplayerCore::Hooks;
using GlobalNamespace::XPlatformAccessTokenData;
using Proof = Oculus::Platform::Models::UserProof;
using ProofMessage = Oculus::Platform::Message_1<Proof*>;
MAKE_AUTO_HOOK_ORIG_MATCH(PlatformAuthenticationTokenProvider_GetXPlatformAccessToken,
    &GlobalNamespace::PlatformAuthenticationTokenProvider::GetXPlatformAccessToken,
    Task<XPlatformAccessTokenData>*, GlobalNamespace::PlatformAuthenticationTokenProvider* self,
    CancellationToken cancellationToken, bool skipCacheRead) {
    if (!NetworkConfigHooks::IsOverridingAPI())
        return PlatformAuthenticationTokenProvider_GetXPlatformAccessToken(self, cancellationToken, skipCacheRead);
    safe_ptr<Task<XPlatformAccessTokenData>*> task(Task<XPlatformAccessTokenData>::New_ctor());
    safe_ptr<System::Threading::CancellationTokenSource*> cancellationSource(cancellationToken._source);
    const std::string userId(self->_userId);
    const std::string graphUrl(NetworkConfigHooks::GetCurrentServer()->graphUrl);
    { std::lock_guard lock(credentialsMutex); credentials = {}; }
    // The legacy master verifies Oculus user proofs, not XPlatform tokens.
    // A failed proof is a failed login; never synthesize a successful credential.
    Oculus::Platform::Users::GetUserProof()->OnComplete(
        custom_types::MakeDelegate<Oculus::Platform::Message_1_Callback<Proof*>*>(
            std::function<void(ProofMessage*)>([task, userId, graphUrl](ProofMessage* message) {
                if (task->IsCompleted) return;
                if (!message || message->IsError || !message->Data || !message->Data->Value || userId.empty()) {
                    ERROR("Could not obtain the Oculus user proof for multiplayer");
                    task->TrySetResult(XPlatformAccessTokenData::getStaticF_invalid());
                    return;
                }
                std::string proof(message->Data->Value);
                { std::lock_guard lock(credentialsMutex); credentials = {graphUrl, userId, proof}; }
                task->TrySetResult(XPlatformAccessTokenData(message->Data->Value, GlobalNamespace::PlatformEnvironment::Production));
            })));
    il2cpp_thread([task, cancellationSource, cancellationToken]() mutable {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!task->IsCompleted && !cancellationToken.IsCancellationRequested && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        if (cancellationToken.IsCancellationRequested) task->TrySetCanceled(cancellationToken);
        else if (!task->IsCompleted) task->TrySetResult(XPlatformAccessTokenData::getStaticF_invalid());
    }).detach();
    return task.ptr();
}
