#include "hooking.hpp"
#include "Networking/MultiplayerTypes.hpp"
#include "logging.hpp"

#include "custom-types/shared/delegate.hpp"
#include "GlobalNamespace/MultiplayerIntroAnimationController.hpp"
#include "GlobalNamespace/MultiplayerPlayersManager.hpp"
#include "GlobalNamespace/IBeatSaberConnectedPlayer.hpp"
#include "UnityEngine/Timeline/TimelineAsset.hpp"
#include "UnityEngine/Timeline/AudioTrack.hpp"
#include "UnityEngine/Playables/PlayableDirector.hpp"
#include "UnityEngine/Playables/PlayableAsset.hpp"
#include "UnityEngine/GameObject.hpp"
#include "System/Action.hpp"
#include "System/Collections/Generic/IReadOnlyList_1.hpp"
#include "System/Collections/Generic/IReadOnlyCollection_1.hpp"

static UnityEngine::Playables::PlayableDirector* originalDirector;
static int iteration = 0;

MAKE_AUTO_HOOK_MATCH(MultiplayerIntroAnimationController_PlayIntroAnimation, &::GlobalNamespace::MultiplayerIntroAnimationController::PlayIntroAnimation, void, GlobalNamespace::MultiplayerIntroAnimationController* self, float maxDesiredIntroAnimationDuration, System::Action* onCompleted) {
    // prefix
    DEBUG("Creating intro PlayableDirector for iteration '{}'", iteration);
    auto originalDirector = self->_introPlayableDirector;

    if (iteration != 0) {
        auto newPlayableGameObject = UnityEngine::GameObject::New_ctor();
        self->_introPlayableDirector = newPlayableGameObject->AddComponent<UnityEngine::Playables::PlayableDirector*>();

        using PlayableDirector_SetPlayableAsset = function_ptr_t<void, UnityEngine::Playables::PlayableDirector*, UnityEngine::ScriptableObject*>;
        static auto playableDirector_SetPlayableAsset = reinterpret_cast<PlayableDirector_SetPlayableAsset>(i2c::functions::resolve_icall("UnityEngine.Playables.PlayableDirector::SetPlayableAsset"));
        playableDirector_SetPlayableAsset(self->_introPlayableDirector, originalDirector->get_playableAsset());

        onCompleted = custom_types::MakeDelegate<System::Action*>(
            std::function<void()>(
                [onCompleted, newPlayableGameObject](){
                    UnityEngine::Object::Destroy(newPlayableGameObject);
                    if (onCompleted) onCompleted->Invoke();
                }
            )
        );
    }

    auto tl = self->_introPlayableDirector->get_playableAsset().cast<UnityEngine::Timeline::TimelineAsset>();
    ArrayW<UnityEngine::Timeline::TrackAsset*> outputTracks(tl->GetOutputTracks());
    for (auto track : outputTracks) {
        auto audioTrack = i2c::try_cast<UnityEngine::Timeline::AudioTrack*>(track);
        track->set_muted((audioTrack != nullptr) && iteration != 0);
    }

    self->_bindingFinished = false;

    // orig call
    MultiplayerIntroAnimationController_PlayIntroAnimation(self, maxDesiredIntroAnimationDuration, onCompleted);

    // postfix
    iteration++;
    self->_introPlayableDirector = originalDirector;
    auto players = self->_multiplayerPlayersManager->get_allActiveAtGameStartPlayers();
    auto c = static_cast<System::Collections::Generic::IReadOnlyCollection_1<::GlobalNamespace::IBeatSaberConnectedPlayer*>*>(*players)->get_Count();
    int playerCount = 0;
    for (int i = 0; i < c; i++) {
        if (!MultiplayerCore::ConnectedPlayer(players->Item[i])->get_isMe())
            playerCount++;
    }

    if (iteration < ((playerCount + 3) / 4)) {
        self->PlayIntroAnimation(maxDesiredIntroAnimationDuration, onCompleted);
    } else iteration = 0;
}

static ListW<GlobalNamespace::IBeatSaberConnectedPlayer*> GetActivePlayersAttacher(ListW<GlobalNamespace::IBeatSaberConnectedPlayer*> allActiveAtGameStartPlayers) {
    // new list
    auto activePlayers = ListW<GlobalNamespace::IBeatSaberConnectedPlayer*>::New();

    // filter for players that are not the local player
    std::vector<GlobalNamespace::IBeatSaberConnectedPlayer*> notMePlayers;
    for (auto p : allActiveAtGameStartPlayers)
        if (!MultiplayerCore::ConnectedPlayer(p)->get_isMe()) notMePlayers.emplace_back(p);

    // skip(iteration * 4).take(4)
    auto start = notMePlayers.begin() + std::min<size_t>(notMePlayers.size(), iteration * 4);
    auto end = start + std::min<ptrdiff_t>(4, notMePlayers.end() - start);

    for (auto itr = start; itr < end && itr < notMePlayers.end(); itr++) {
        activePlayers->Add(*itr);
    }

    if (iteration == 0) {
        auto me = std::find_if(allActiveAtGameStartPlayers.begin(), allActiveAtGameStartPlayers.end(), [](auto p){ return MultiplayerCore::ConnectedPlayer(p)->get_isMe(); });
        if (me != allActiveAtGameStartPlayers.end())
            activePlayers->Add(*me);
    }

    return activePlayers;
}

MAKE_AUTO_HOOK_MATCH(MultiplayerIntroAnimationController_BindTimeline, &::GlobalNamespace::MultiplayerIntroAnimationController::BindTimeline, void, GlobalNamespace::MultiplayerIntroAnimationController* self) {
    // the game early returns on this bool. since we also are going to be doing things it's best to also check the same bool for early return
    if (self->_bindingFinished) {
        MultiplayerIntroAnimationController_BindTimeline(self);
        return;
    }

    // save original data for restoring later
    auto originalData = self->_multiplayerPlayersManager->allActiveAtGameStartPlayers;
    self->_multiplayerPlayersManager->_allActiveAtGameStartPlayers = *static_cast<System::Collections::Generic::List_1<::GlobalNamespace::IBeatSaberConnectedPlayer*>*>(GetActivePlayersAttacher(originalData));

    // call original
    MultiplayerIntroAnimationController_BindTimeline(self);
    // restore original data back to manager
    self->_multiplayerPlayersManager->_allActiveAtGameStartPlayers = originalData;
}
