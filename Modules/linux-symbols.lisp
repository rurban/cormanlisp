;;;	-------------------------------
;;;	Copyright (c) Corman Technologies Inc.
;;;	See LICENSE.txt for license information.
;;;	-------------------------------
;;;
;;;	File:		linux-symbols.lisp
;;;	Contents:	FFI bindings for Linux libc and system calls.
;;;	            Replaces win32-symbols.lisp on Linux.
;;;	History:	2026  Linux port.
;;;

;; ---- Dynamic linker ----
(defun-kernel dlopen (name flags)
	(:handle %dlopen :c-name "dlopen"))

(defun-kernel dlsym (handle name)
	(:handle %dlsym :c-name "dlsym"))

(defun-kernel dlclose (handle)
	(:int %dlclose :c-name "dlclose"))

(defun-kernel dlerror ()
	(:string %dlerror :c-name "dlerror"))

;; ---- File I/O (libc) ----
(defun-kernel open (pathname flags &optional mode)
	(:int %open :c-name "open"))

(defun-kernel close (fd)
	(:int %close :c-name "close"))

(defun-kernel read (fd buf count)
	(:long %read :c-name "read"))

(defun-kernel write (fd buf count)
	(:long %write :c-name "write"))

(defun-kernel lseek (fd offset whence)
	(:long %lseek :c-name "lseek"))

(defun-kernel stat (path buf)
	(:int %stat :c-name "stat"))

(defun-kernel fstat (fd buf)
	(:int %fstat :c-name "fstat"))

(defun-kernel unlink (path)
	(:int %unlink :c-name "unlink"))

(defun-kernel rename (old new)
	(:int %rename :c-name "rename"))

(defun-kernel chdir (path)
	(:int %chdir :c-name "chdir"))

(defun-kernel getcwd (buf size)
	(:string %getcwd :c-name "getcwd"))

(defun-kernel mkdir (path mode)
	(:int %mkdir :c-name "mkdir"))

(defun-kernel rmdir (path)
	(:int %rmdir :c-name "rmdir"))

;; ---- Process control ----
(defun-kernel getpid ()
	(:long %getpid :c-name "getpid"))

(defun-kernel getppid ()
	(:long %getppid :c-name "getppid"))

(defun-kernel fork ()
	(:long %fork :c-name "fork"))

(defun-kernel execve (path argv envp)
	(:int %execve :c-name "execve"))

(defun-kernel waitpid (pid status options)
	(:long %waitpid :c-name "waitpid"))

(defun-kernel exit (status)
	(:void %exit :c-name "exit"))

(defun-kernel system (command)
	(:int %system :c-name "system"))

;; ---- Memory management ----
(defun-kernel mmap (addr length prot flags fd offset)
	(:handle %mmap :c-name "mmap"))

(defun-kernel munmap (addr length)
	(:int %munmap :c-name "munmap"))

(defun-kernel mprotect (addr length prot)
	(:int %mprotect :c-name "mprotect"))

;; ---- Time ----
(defun-kernel time (tloc)
	(:long %time :c-name "time"))

(defun-kernel gettimeofday (tv tz)
	(:int %gettimeofday :c-name "gettimeofday"))

(defun-kernel clock-gettime (clk_id tp)
	(:int %clock_gettime :c-name "clock_gettime"))

;; ---- Signals ----
(defun-kernel signal (sig handler)
	(:handle %signal :c-name "signal"))

(defun-kernel sigaction (sig act oact)
	(:int %sigaction :c-name "sigaction"))

(defun-kernel kill (pid sig)
	(:int %kill :c-name "kill"))

(defun-kernel raise (sig)
	(:int %raise :c-name "raise"))

;; ---- Sockets ----
(defun-kernel socket (domain type protocol)
	(:int %socket :c-name "socket"))

(defun-kernel connect (sockfd addr addrlen)
	(:int %connect :c-name "connect"))

(defun-kernel bind (sockfd addr addrlen)
	(:int %bind :c-name "bind"))

(defun-kernel listen (sockfd backlog)
	(:int %listen :c-name "listen"))

(defun-kernel accept (sockfd addr addrlen)
	(:int %accept :c-name "accept"))

(defun-kernel send (sockfd buf len flags)
	(:long %send :c-name "send"))

(defun-kernel recv (sockfd buf len flags)
	(:long %recv :c-name "recv"))

(defun-kernel shutdown (sockfd how)
	(:int %shutdown :c-name "shutdown"))

(defun-kernel setsockopt (sockfd level optname optval optlen)
	(:int %setsockopt :c-name "setsockopt"))

(defun-kernel getsockname (sockfd addr addrlen)
	(:int %getsockname :c-name "getsockname"))

(defun-kernel getpeername (sockfd addr addrlen)
	(:int %getpeername :c-name "getpeername"))

;; ---- Environment ----
(defun-kernel getenv (name)
	(:string %getenv :c-name "getenv"))

(defun-kernel setenv (name value overwrite)
	(:int %setenv :c-name "setenv"))

(defun-kernel unsetenv (name)
	(:int %unsetenv :c-name "unsetenv"))

;; ---- Pthreads ----
(defun-kernel pthread-create (thread attr start_routine arg)
	(:int %pthread_create :c-name "pthread_create"))

(defun-kernel pthread-join (thread value_ptr)
	(:int %pthread_join :c-name "pthread_join"))

(defun-kernel pthread-self ()
	(:long %pthread_self :c-name "pthread_self"))

(defun-kernel pthread-mutex-init (mutex attr)
	(:int %pthread_mutex_init :c-name "pthread_mutex_init"))

(defun-kernel pthread-mutex-lock (mutex)
	(:int %pthread_mutex_lock :c-name "pthread_mutex_lock"))

(defun-kernel pthread-mutex-unlock (mutex)
	(:int %pthread_mutex_unlock :c-name "pthread_mutex_unlock"))
