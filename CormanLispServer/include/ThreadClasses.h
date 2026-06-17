//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ThreadClasses.h
//		Contents:	Thread synchronization classes for Corman Lisp.
//		            Linux port: Win32 primitives replaced with pthreads.
//		History:	8/5/97  RGC Created.
//

#ifndef THREADCLASSES_H
#define THREADCLASSES_H

#include "platform.h"
#include <assert.h>

// ---- CriticalSection (pthread_mutex_t) ----
// Non-recursive mutex; the original Win32 CRITICAL_SECTION is also non-recursive.
class CriticalSection
{
public:
	CriticalSection()  { pthread_mutex_init(&m_mutex, NULL); }
	~CriticalSection() { pthread_mutex_destroy(&m_mutex); }
	void Enter()       { pthread_mutex_lock(&m_mutex); }
	void Leave()       { pthread_mutex_unlock(&m_mutex); }
public:
	pthread_mutex_t m_mutex;
	char m_sect[1];    // backward compat: Win32 CRITICAL_SECTION alias
};

class ScopedLock
{
public:
	ScopedLock(CriticalSection &cs) : m_cs(cs) { m_cs.Enter(); }
	~ScopedLock() { m_cs.Leave(); }
public:
	CriticalSection &m_cs;
};

// ---- PLSyncObject (base class) ----
class PLSingleLock;
class PLSyncObject
{
public:
	PLSyncObject(LPCTSTR pstrName);
	virtual ~PLSyncObject();

	HANDLE  m_hObject;       // kept for API compat; in Linux port this is
	                         // a pthread_mutex_t* or sem_t* cast to HANDLE
	operator HANDLE() const { return m_hObject; }

	virtual BOOL Lock(DWORD dwTimeout = INFINITE);
	virtual BOOL Unlock() = 0;
	virtual BOOL Unlock(LONG, LPLONG) { return TRUE; }

	friend class PLSingleLock;
};

// ---- PLEvent (manual/auto-reset event via pthread_cond_t) ----
class PLEvent : public PLSyncObject
{
public:
	PLEvent(BOOL bInitiallyOwn = FALSE, BOOL bManualReset = FALSE,
		LPCTSTR lpszName = NULL, LPSECURITY_ATTRIBUTES lpsaAttribute = NULL);
	virtual ~PLEvent();

	BOOL SetEvent();
	BOOL PulseEvent();
	BOOL ResetEvent();
	BOOL Unlock();

	pthread_mutex_t m_eventMutex;
	pthread_cond_t  m_eventCond;
	bool            m_signaled;
	bool            m_manualReset;
};

// ---- PLSemaphore (POSIX semaphore) ----
class PLSemaphore : public PLSyncObject
{
public:
	PLSemaphore(LONG lInitialCount = 1, LONG lMaxCount = 1,
		LPCTSTR pstrName = NULL, LPSECURITY_ATTRIBUTES lpsaAttributes = NULL);
	virtual ~PLSemaphore();

	virtual BOOL Unlock();
	virtual BOOL Unlock(LONG lCount, LPLONG lprevCount = NULL);

	sem_t m_sem;
};

// ---- PLSingleLock (RAII lock wrapper) ----
class PLSingleLock
{
public:
	PLSingleLock(PLSyncObject* pObject, BOOL bInitialLock = FALSE);
	~PLSingleLock();

	BOOL Lock(DWORD dwTimeOut = INFINITE);
	BOOL Unlock();
	BOOL Unlock(LONG lCount, LPLONG lPrevCount = NULL);
	BOOL IsLocked();

protected:
	PLSyncObject* m_pObject;
	HANDLE  m_hObject;
	BOOL    m_bAcquired;
};

// ---- Inline implementations ----

inline PLSingleLock::~PLSingleLock() { Unlock(); }
inline BOOL PLSingleLock::IsLocked() { return m_bAcquired; }

inline BOOL PLSemaphore::Unlock()
	{ return Unlock(1, NULL); }

inline BOOL PLEvent::SetEvent()
{
	pthread_mutex_lock(&m_eventMutex);
	m_signaled = true;
	pthread_cond_broadcast(&m_eventCond);
	pthread_mutex_unlock(&m_eventMutex);
	return TRUE;
}

inline BOOL PLEvent::ResetEvent()
{
	pthread_mutex_lock(&m_eventMutex);
	m_signaled = false;
	pthread_mutex_unlock(&m_eventMutex);
	return TRUE;
}

#endif // THREADCLASSES_H
