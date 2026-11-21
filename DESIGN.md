
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

BuildingState::workspace_changed() cancels the obsolete build.The GUI displays “Corpus: Not built” and disables processing. CancellingState keeps Build disabled until completion.

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










# M4 Decision Record

<!-- See specification Section 8. -->
<!-- a short decision record naming your own classes and functions: (1)
threads and ownership, including the statement for constraint 5; (2) how a finished
build is judged valid; (3) changes since the checkpoint; (4) CR-1 impact, meaning
which functions changed and why the rest did not; (5) where each fault switch is and
which test catches it; (6) AI-use disclosure, required but ungraded -->



## Threads and ownership

MainWinodw owns the workspace and ProcessingCore as clarified in previous milestones, and now it also owns the build thread. build_corpus() copies the workspace and moves the snapshot into worker. The worker biulds its own ProcessingCore and transfers it through a promise or stores an exception in the promise if building fails. Wherein a GUI thread QTimer calls finish_corpus build that checks the future. It does so without blocking and even joins the worker once there's a result, and the GUI takes ownership of accepted core. A discarded result is destroyed locally too. 

The GUI writes the build's atomic canellation flag and the worker reads it. Build delay and cancellation in build_control.hpp are local to the thread. IPC server's atomic stop flag is written during shutdown and read by socket thread, build results corss threads via promise futre, and remote add requests cross over Qt's queued invoking. 

## Corpus validity and changes since the checkpoint

build_corpus() records version of the workspace, and a successfull add/reset increments that version and does the job of corpus invalidation. Finish_corpus_build() installs completed core if and only if its version matches still. Exit (confirmed) and reset request cancellation of course. If exit occurs during a build, closeEvent() defers closing later for once a worker joined. Was proposed a separate `BuildController` but I kept the worker, timer, and completion handler in `MainWindow` due to  corpus acceptance/widget updates/ deferred closing/ etc being coordinated there. ANd yet, the worker still builds from its own workspace snapshot.

## CR-1 remote add


main() starts an IPC server on a separate thread. the server opens the requested file and posts add_document_file() to the GUI thread, replying w/o waiting for the GUI to process. add_document_file() is shared by local and remote, meaning that they use exact same workspace version plus rules regarding invalidation. WIth this, CR1 reuses the existing build and search logic. Once shutdown, server thread is joined and i've verified the the socket file does get removed.

One reply begins with a 4 byte payload lengthn in big endian, followed by that many bytes with a max of 65536 bytes. Aiws_send resolves relative paths against its own directory with 0 meaning a successful reply and 1 signaling the error source. Socket helpers loop over such that path bytes may include spaces, new lines, and UTF8

## Verification

During my testing, student_tests run passed, AIWS_FAULT_ACCEPT_STALE bypasses the version check in finish_corpus_build() and was caught by discards_build_after_document_added; and, AIWS_FAULT_LOSE_FAILURE skips failure reporting. The action refreshes in the same function and thus caught by reports_build_failure_and_recovers as intended. I got the results expected and intended within a 60 second window. 

## AI-use disclosure

I asked Google for a list of relevant docker and powershell commands and it's 'AI mode' suggested some. When I accidentally committed m4 first changes to m3 in Github it told me the git commands to remove and check that the hash was the same in gradescope as on github. I built and tested the implementation in the course Docker container myself.

## Pair programming log

- **Partner:** Cristian Jimenez-Ebert
- **Session 1:** Nov. 6, 2026, 12:00 PM–6:40 p.m. We worked on my M4 code, focusing on what happens when a document is added during a corpus build.
- **Session 2:** Nov. 7, 2026, 12:00–5:40 p.m. We worked on Cristian's M4 code, focused on how the cr1 server reads a request split across multiple writes.
- **My contribution as navigator:** I pointed out that one socket read might  return only part of the 4 byte length and suggested looping until allfour bytes had arrived.
- **My partner's contribution as navigator:** Cristian suggested waiting for  Build Corpus to become enabled again before checking the corpus status. This, the test wouldnt pass while the old build was still running.
- **What worked best:** Talking through each event in order helped us catch c/ases that were easy to miss when reading the code one function at a time