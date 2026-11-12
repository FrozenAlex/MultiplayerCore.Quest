#include "hooking.hpp"
#include "tasks.hpp"
#include "Hooks/NetworkConfigHooks.hpp"
#include "Hooks/LegacyAuthentication.hpp"
#include "Hooks/LegacyMatchmakingJson.hpp"
#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/HttpRequestMessage.hpp"
#include "System/Net/Http/HttpResponseMessage.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/AuthenticationHeaderValue.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Net/Http/StringContent.hpp"
#include "System/Text/Encoding.hpp"
#include "System/Uri.hpp"
#include "beatsaber-hook/shared/rapidjson.hpp"
using namespace System::Net::Http;
using namespace MultiplayerCore;
using namespace MultiplayerCore::Hooks;
MAKE_AUTO_HOOK_ORIG_MATCH(HttpClient_SendAsync, &HttpClient::SendAsync,
    Task<HttpResponseMessage*>*, HttpClient* self, HttpRequestMessage* request,
    HttpCompletionOption completionOption, CancellationToken cancellationToken) {
    auto cfg = NetworkConfigHooks::GetCurrentServer();
    if (!NetworkConfigHooks::IsOverridingAPI() || !cfg || !request || !request->RequestUri)
        return HttpClient_SendAsync(self, request, completionOption, cancellationToken);
    std::string uri(request->RequestUri->AbsoluteUri);
    std::string base = cfg->graphUrl;
    while (!base.empty() && base.back() == '/') base.pop_back();
    // Exact endpoint matching keeps credentials out of unrelated requests.
    const auto endpoint = uri.substr(0, uri.find('?'));
    if (endpoint == base + "/beat_saber_multiplayer_status") {
        request->RequestUri = System::Uri::New_ctor(cfg->masterServerStatusUrl);
        return HttpClient_SendAsync(self, request, completionOption, cancellationToken);
    }
    if (endpoint != base + "/beat_saber_get_multiplayer_instance" || !request->Content)
        return HttpClient_SendAsync(self, request, completionOption, cancellationToken);
    const auto credentials = GetLegacyCredentials();
    safe_ptr<HttpClient*> client(self);
    safe_ptr<HttpRequestMessage*> rootedRequest(request);
    return StartTask<HttpResponseMessage*>([client, rootedRequest, credentials, completionOption, base](CancellationToken token) {
        // Quest strips the Authorization getter. GraphAPIClient's setter stores a parsed value here.
        System::Net::Http::Headers::HttpHeaders::HeaderBucket* bucket = nullptr;
        auto headers = rootedRequest->Headers->headers;
        if (headers) headers->TryGetValue("Authorization", by_ref(bucket));
        auto authorization = bucket ? i2c::try_cast<System::Net::Http::Headers::AuthenticationHeaderValue*>(bucket->Parsed) : nullptr;
        const std::string proof = authorization ? std::string(authorization->Parameter) : std::string();
        if (credentials.graphUrl.empty() || credentials.userId.empty() || proof.empty())
            throw std::runtime_error("No Oculus multiplayer proof available");
        std::string credentialBase = credentials.graphUrl;
        while (!credentialBase.empty() && credentialBase.back() == '/') credentialBase.pop_back();
        if (credentialBase != base) throw std::runtime_error("Multiplayer server changed during authentication");
        std::string body(Await(rootedRequest->Content->ReadAsStringAsync(), token));
        // Use this request's proof; concurrent token requests may update the credential cache.
        const auto rewritten = AddLegacyCredentials(body, credentials.userId, proof);
        rootedRequest->Content = StringContent::New_ctor(rewritten, System::Text::Encoding::get_UTF8(), "application/json");
        return Await(HttpClient_SendAsync(client.ptr(), rootedRequest.ptr(), completionOption, token), token);
    }, cancellationToken);
}
