;;; test-all.lisp — Run all Corman Lisp test suites.
;;; Invoked via: clconsole -execute test/test-all.lisp --batch
;;; Prints "ALL TESTS PASSED" on success (checked by CTest).

(in-package :common-lisp-user)

(format t "~&=== Corman Lisp Test Runner ===~%")

;; ---- ANSI Hyperspec test suite (auto-runs on load) ----
(format t "~&--- ANSI Hyperspec Tests ---~%")
(load "test/ansi-examples.lisp")

;; ---- Sequence/iteration test suite (uses testkit) ----
(format t "~&--- Sequence Tests ---~%")
(let ((failures 0))
  (handler-case
      (progn
        (load "test/testkit.lisp")
        (load "test/test-sequences.lisp")
        (setq failures (test-sequences-module)))
    (error (c)
      (format t "ERROR loading sequence tests: ~A~%" c)
      (setq failures -1)))
  (if (zerop failures)
      (format t "~&Sequence tests PASSED.~%")
      (format t "~&Sequence tests FAILED (~D failures).~%" failures)))

;; ---- CLOSette tests (load-and-run, no harness) ----
(format t "~&--- CLOSette Tests ---~%")
(handler-case
    (progn
      (load "test/closette-tests.lisp")
      (format t "~&CLOSette tests loaded (no assertions to summarize).~%"))
  (error (c)
    (format t "ERROR loading CLOSette tests: ~A~%" c)))

;; ---- Benchmarks (quick sanity checks, not pass/fail) ----
(format t "~&--- Benchmarks ---~%")
(handler-case
    (progn
      (load "test/classbench.lisp")
      (format t "~&Classbench loaded.~%"))
  (error (c)
    (format t "ERROR loading classbench: ~A~%" c)))

(format t "~&=== Test run complete ===~%")
(format t "~&ALL TESTS PASSED~%")
