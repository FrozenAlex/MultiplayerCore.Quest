#pragma once
#include "beatsaber-hook/shared/threading.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "System/Exception.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Runtime/CompilerServices/TaskAwaiter_1.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include <chrono>
#include <thread>
namespace MultiplayerCore {
template<class T> using Task = System::Threading::Tasks::Task_1<T>;
using System::Threading::CancellationToken;
// Root the task across the native worker and always complete it on failure.
template<class Ret, class F> requires std::is_invocable_r_v<Ret, F>
Task<Ret>* StartTask(F&& function) {
    safe_ptr<Task<Ret>*> task(Task<Ret>::New_ctor());
    il2cpp_thread([task, function = std::forward<F>(function)]() mutable {
        try { task->TrySetResult(function()); }
        catch (const std::exception& ex) { task->TrySetException(System::Exception::New_ctor(ex.what())); }
        catch (...) { task->TrySetException(System::Exception::New_ctor("MultiplayerCore task failed")); }
    }).detach();
    return task.ptr();
}
template<class Ret, class F> requires std::is_invocable_r_v<Ret, F, CancellationToken>
Task<Ret>* StartTask(F&& function, CancellationToken cancelToken) {
    safe_ptr<Task<Ret>*> task(Task<Ret>::New_ctor());
    safe_ptr<System::Threading::CancellationTokenSource*> source(cancelToken._source);
    il2cpp_thread([task, source, cancelToken, function = std::forward<F>(function)]() mutable {
        try {
            if (cancelToken.IsCancellationRequested) { task->TrySetCanceled(cancelToken); return; }
            auto result = function(cancelToken);
            if (cancelToken.IsCancellationRequested) task->TrySetCanceled(cancelToken);
            else task->TrySetResult(result);
        } catch (const std::exception& ex) {
            if (cancelToken.IsCancellationRequested) task->TrySetCanceled(cancelToken);
            else task->TrySetException(System::Exception::New_ctor(ex.what()));
        } catch (...) {
            if (cancelToken.IsCancellationRequested) task->TrySetCanceled(cancelToken);
            else task->TrySetException(System::Exception::New_ctor("MultiplayerCore task failed"));
        }
    }).detach();
    return task.ptr();
}
template<class Ret, class F, class... Args> auto StartThread(F&& fn, Args&&... args) {
    return StartTask<Ret>(std::forward<F>(fn), std::forward<Args>(args)...);
}
template<class Ret, class F, class... Args> auto StartSafePtrThread(F&& fn, Args&&... args) {
    return safe_ptr<Task<Ret>*>(StartTask<Ret>(std::forward<F>(fn), std::forward<Args>(args)...));
}
template<class T> T Await(Task<T>* raw, CancellationToken token = {}) {
    safe_ptr<Task<T>*> task(raw);
    while (!task->IsCompleted) {
        token.ThrowIfCancellationRequested();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return task->GetAwaiter().GetResult();
}
}
