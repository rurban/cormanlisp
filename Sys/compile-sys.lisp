;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
(progn (terpri)(write "Creating the file CormanLisp.img")(terpri) (values))

;; change current directory to the Corman Lisp directory
(progn
  (terpri)
  (write "Changing current directory to ")
  (write (%change-directory (%cormanlisp-directory-namestring)))
  (terpri)
  (values))
(progn (load "Sys/load-sys.lisp")(values))
(top-level)
(progn (load "Sys/load-sys2.lisp")(values))
(setf ccl::*save-relative-source-file-names* nil)
(progn (in-package :user)(values))
