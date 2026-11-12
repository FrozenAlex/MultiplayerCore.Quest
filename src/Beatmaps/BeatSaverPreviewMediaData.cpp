#include "Beatmaps/BeatSaverPreviewMediaData.hpp"
#include "logging.hpp"
#include "tasks.hpp"
#include "UnityEngine/Networking/UnityWebRequestMultimedia.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestAsyncOperation.hpp"
#include "UnityEngine/Networking/DownloadHandlerAudioClip.hpp"
#include "UnityEngine/AudioType.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"

DEFINE_TYPE(MultiplayerCore::Beatmaps, BeatSaverPreviewMediaData);
using namespace System::Threading::Tasks;
namespace MultiplayerCore::Beatmaps {
    void BeatSaverPreviewMediaData::ctor(StringW levelHash) {
        INVOKE_CTOR();
        _levelHash = std::string(levelHash);
    }

    template<class F> bool WithVersion(const std::string& hash, F&& callback) {
        auto response = BeatSaver::API::GetBeatmapByHash(hash);
        if (!response.DataParsedSuccessful()) return false;
        for (const auto& version : response.responseData->GetVersions()) {
            if (std::ranges::equal(version.GetHash(), hash, [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            })) { callback(version); return true; }
        }
        return false;
    }

    Task_1<UnityEngine::Sprite*>* BeatSaverPreviewMediaData::GetCoverSpriteAsync() {
        if (_cachedCoverImage && !_cachedCoverImage->m_CachedPtr.IsNull())
            return Task_1<UnityEngine::Sprite*>::FromResult(_cachedCoverImage);
        if (_coverImageTask) return _coverImageTask;
        safe_ptr<BeatSaverPreviewMediaData*> owner(this);
        safe_ptr<Task_1<UnityEngine::Sprite*>*> task(Task_1<UnityEngine::Sprite*>::New_ctor());
        _coverImageTask = task.ptr();
        const int generation = ++_coverGeneration;
        il2cpp_thread([owner, task, hash = _levelHash, generation] {
            try {
                std::vector<uint8_t> bytes;
                WithVersion(hash, [&bytes](const auto& version) {
                    if (auto image = version.GetCoverImage()) bytes.assign(image->begin(), image->end());
                });
                BSML::MainThreadScheduler::Schedule([owner, task, generation, bytes = std::move(bytes)] {
                    try {
                        if (owner->_coverGeneration != generation || bytes.empty()) { task->TrySetResult(nullptr); return; }
                        owner->_cachedCoverImage = BSML::Utilities::LoadSpriteRaw(ArrayW<uint8_t>(bytes));
                        task->TrySetResult(owner->_cachedCoverImage);
                    } catch (...) { task->TrySetResult(nullptr); }
                });
            } catch (...) { task->TrySetResult(nullptr); }
        }).detach();
        return task.ptr();
    }

    Task_1<UnityEngine::AudioClip*>* BeatSaverPreviewMediaData::GetPreviewAudioClip() {
        if (_cachedAudioClip && !_cachedAudioClip->m_CachedPtr.IsNull())
            return Task_1<UnityEngine::AudioClip*>::FromResult(_cachedAudioClip);
        if (_audioClipTask) return _audioClipTask;
        safe_ptr<BeatSaverPreviewMediaData*> owner(this);
        safe_ptr<Task_1<UnityEngine::AudioClip*>*> task(Task_1<UnityEngine::AudioClip*>::New_ctor());
        _audioClipTask = task.ptr();
        const int generation = ++_audioGeneration;
        il2cpp_thread([owner, task, hash = _levelHash, generation] {
            try {
                std::string url;
                WithVersion(hash, [&url](const auto& version) { url = version.GetPreviewURL(); });
                if (url.empty()) { task->TrySetResult(nullptr); return; }
                BSML::MainThreadScheduler::Schedule([owner, task, generation, url = std::move(url)] {
                    try { BSML::SharedCoroutineStarter::StartCoroutine(LoadAudio(safe_ptr<System::Object*>(owner.ptr()), task, url, generation)); }
                    catch (...) { task->TrySetResult(nullptr); }
                });
            } catch (...) { task->TrySetResult(nullptr); }
        }).detach();
        return task.ptr();
    }

    custom_types::Helpers::Coroutine BeatSaverPreviewMediaData::LoadAudio(safe_ptr<System::Object*> rootedOwner,
        safe_ptr<Task_1<UnityEngine::AudioClip*>*> task, std::string url, int generation) {
        auto owner = static_cast<BeatSaverPreviewMediaData*>(rootedOwner.ptr());
        // All Unity networking and asset access stays on the main thread.
        safe_ptr<UnityEngine::Networking::UnityWebRequest*> request;
        try {
            if (owner->_audioGeneration != generation) { task->TrySetResult(nullptr); co_return; }
            request = UnityEngine::Networking::UnityWebRequestMultimedia::GetAudioClip(url, UnityEngine::AudioType::MPEG);
            request->timeout = 20;
            safe_ptr<UnityEngine::Networking::UnityWebRequestAsyncOperation*> operation(request->SendWebRequest());
            while (!operation->isDone && owner->_audioGeneration == generation) co_yield nullptr;
            if (owner->_audioGeneration != generation) {
                request->Abort();
                task->TrySetResult(nullptr);
            } else {
                owner->_cachedAudioClip = UnityEngine::Networking::DownloadHandlerAudioClip::GetContent(request.ptr());
                task->TrySetResult(owner->_cachedAudioClip);
            }
        } catch (...) { task->TrySetResult(nullptr); }
        if (request) request->Dispose();
    }

    void BeatSaverPreviewMediaData::UnloadCoverSprite() {
        ++_coverGeneration;
        if (_cachedCoverImage && !_cachedCoverImage->m_CachedPtr.IsNull()) {
            if (_cachedCoverImage->texture && !_cachedCoverImage->texture->m_CachedPtr.IsNull())
                UnityEngine::Object::Destroy(_cachedCoverImage->texture);
            UnityEngine::Object::Destroy(_cachedCoverImage);
        }
        _cachedCoverImage = nullptr;
        _coverImageTask = nullptr;
    }
    void BeatSaverPreviewMediaData::UnloadPreviewAudioClip() {
        ++_audioGeneration;
        if (_cachedAudioClip && !_cachedAudioClip->m_CachedPtr.IsNull()) UnityEngine::Object::Destroy(_cachedAudioClip);
        _cachedAudioClip = nullptr;
        _audioClipTask = nullptr;
    }
}
