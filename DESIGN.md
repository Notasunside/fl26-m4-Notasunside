
# M5 Design

## 1. What M4 problems did you fix?


<!--done -->
I corrected Server::handle_client() based on the TA's autograder feedback such that all queue failure replies begin with
ASCII '1' now. I missed one last time. I also expanded tests for the many following: worker reuse, shutdown, remote additions,
malformed requests, and client exit codes since my previous list was not exhaustive and didn't include any CR-1 test, much to my embarassment because i thought i pushed it after the peer meetings. 

## 2. What threads and shared mutable data exist, and why are they safe?


<!--done -->
As stated many times before, GUI thread owns the workspace, installed ProcessingCore, widgets, and the states fully/only.
The build thread starts on the first build and remains alive all the way until shutdown, and in M4 it was clarified that server has one separate IPC thread. There are two main threads in my full implementation from that information.
build_mutex_ protects pending_build_ and build_worker_stopping_. build_ready_ also wakes the worker for requests or shutdown. As for those requests, they contain
a Workspace snapshot/ version/ delay/ and shared atomic cancellation flag which I learned to be good/effective practice during lectures. The worker moves also each request into 
local storage before building anything too. The GUI even writes the cancellation flag and the worker reads it.

The worker fills the result holder before posting handle_build_result() through a queued Qt callback, then stops accessing its contents. Queued delivery synchronizes the handoff. Server::stop_ is atomic, plus thread-local socket_stop_flag points to that atomic. Once it's all said and done, IPC additions are queued to the GUI thread.

## 3. What are your states, their meanings, and transitions?

<!--done -->
State & Meaning + Transitions

NotBuiltState means that there is no useable corpus, and the build transitions to BuildingState.
BuildingState means building. Changes or reset transitions to CancellingState. A current success transitions to CurrentState. Failure transitions to NotBuiltState (and the cycle repeats in that case).
Cancelling state means obselete work is stopping now. Completion transitions to NotBuiltState. 
CurrentState means processing is allowed. Build then transitions to BuildingState, meanwhile changes or reset transition to NotBuiltState.
Closing state means shutdown is underway. The lifecycle events are entirely ingored. No transition because confirmed close enters ClosingState from every active state. 
All of them are called/created/destroyed on the GUI thread only.

## 4. What event sequence threatens R1–R4, and how do your functions handle it?


<!--done -->
The are multiple functions that handle the threat to those requirements.
One threat is when a remote add arrives during a build. To handle it, server queues add_document_file() on the GUI thread. It then increments workspace_version_ and dispatches
workspace_changed(). BuildingState requests cancellation and enters
CancellingState, as mentioned to be the intended path previously. The worker detects cancellation in check_build_cancelled() and posts completion. CancellingState::completed() discards the result and enters NotBuiltState. Then next request uses the same worker. If Exit occurs at any point during this sequence, stop_build_worker() cancels work, ssignals shutdown, wakes the worker, and joins it together. This is how the functions handle that threat.

## 5. What are your three design decisions?


<!--done -->

### May the user request Build during a build?

Build requests during BuildingState become ignored, CancellingState::build() accepts a replacement request through start_build() and transitions to BuildingState, the worker takes the replacement after cancellation, and handle_build_result() ignores completions belonging to any and all older requests.

### What happens when a remote add arrives mid-build?

BuildingState::workspace_changed() cancels the obsolete build.The GUI displays “Corpus: Not built” and disables processing. So in CancellingState, build is enabled when the workspace contains documents. the user can queue a replacement build, and the worker starts it after the cancelled build stops.

### What is the smallest shared data set, and why is it needed?

The pending request carries input, the shutdown flag stops idle work, the atomic cancellation flag stops active work, each supporting a different worker operation, states and widgets stay on the GUI thread.

## 6. Where are the fault switches, and which tests catch them?


<!--done -->

I added two. AIWS_FAULT_NO_CANCEL disables check_build_cancelled() in build_control.hpp. reset_cancels_running_build() detects it. For the second, AIWS_FAULT_SILENT_SHUTDOWN changes build_worker_loop()'s wait predicate. And, exits_after_worker_becomes_idle() detects it with a bounded child process.

## 7. How did you use AI?


<!--done -->

I used Google to remember all the branch relevant commands for Git and Github because I had forgotten them. When I accidentally merged a pulled request I used Google to learn how to revert the changes.
I also used Google to give me a list of powershell commands for docker when running tests.
I used google to understand compilation and build errors I would get that I couldn't recognize for the life of me because I had never run into them before.

