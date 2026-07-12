;;; test-all.lisp — Minimal smoke test for Corman Lisp Linux port.
;;; Prints "ALL TESTS PASSED" on success (checked by CTest).

(in-package :common-lisp-user)

(format t "~%=== Corman Lisp Test Runner ===~%")
(force-output)

;; Basic arithmetic
(let ((result (+ 1 2)))
  (unless (= result 3)
    (error "FAIL: (+ 1 2) => ~A, expected 3" result))
  (format t "Arithmetic: PASS~%"))

;; List operations
(let ((result (cons 'a 'b)))
  (unless (and (consp result) (eq (car result) 'a) (eq (cdr result) 'b))
    (error "FAIL: (cons 'a 'b) => ~A" result))
  (format t "Lists: PASS~%"))

;; String operations
(let ((s "hello"))
  (unless (and (stringp s) (= (length s) 5))
    (error "FAIL: string length"))
  (format t "Strings: PASS~%"))

;; Array creation
(let ((v (make-array 3)))
  (unless (vectorp v)
    (error "FAIL: (make-array 3) returned non-vector ~A" (type-of v)))
  (format t "Arrays: PASS~%"))

;; Output
(format t "Output: ")
(write-string "PASS")
(terpri)

(format t "~%=== Test run complete ===~%")
(format t "ALL TESTS PASSED~%")
