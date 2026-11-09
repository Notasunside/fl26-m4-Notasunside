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
