//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ThreadClasses.cpp
//		Contents:	Thread synchronization classes for Corman Lisp.
//		            Windows: native CRITICAL_SECTION/Event/Semaphore.
//		            Linux:   Win32 primitives replaced with pthreads.
//		History:	8/5/97  RGC Created.
//

#include "Stdafx.h"
#include "ThreadClasses.h"
#include <assert.h>

#ifdef _WIN32

PLEvent::PLEvent(BOOL bInitiallyOwn, BOOL bManualReset, LPCTSTR pstrName,
	LPSECURITY_ATTRIBUTES lpsaAttribute)
	: PLSyncObject(pstrName)
{
	m_hObject = ::CreateEvent(lpsaAttribute, bManualReset,
		bInitiallyOwn, pstrName);
}

PLEvent::~PLEvent()
{
}

BOOL PLEvent::Unlock()
{
	return TRUE;
}

PLSemaphore::PLSemaphore(LONG lInitialCount, LONG lMaxCount,
	LPCTSTR pstrName, LPSECURITY_ATTRIBUTES lpsaAttributes)
	:  PLSyncObject(pstrName)
{
	m_hObject = ::CreateSemaphore(lpsaAttributes, lInitialCount, lMaxCount,
		pstrName);
}

PLSemaphore::~PLSemaphore()
{
}

BOOL PLSemaphore::Unlock(LONG lCount, LPLONG lpPrevCount /* =NULL */)
{
	return ::ReleaseSemaphore(m_hObject, lCount, lpPrevCount);
}

PLSingleLock::PLSingleLock(PLSyncObject* pObject, BOOL bInitialLock)
{
	m_pObject = pObject;
	m_hObject = pObject->m_hObject;
	m_bAcquired = FALSE;

	if (bInitialLock)
		Lock();
}

BOOL PLSingleLock::Lock(DWORD dwTimeOut /* = INFINITE */)
{
	assert(!m_bAcquired);

	m_bAcquired = m_pObject->Lock(dwTimeOut);
	return m_bAcquired;
}

BOOL PLSingleLock::Unlock()
{
	if (m_bAcquired)
		m_bAcquired = !m_pObject->Unlock();

	// successfully unlocking means it isn't acquired
	return !m_bAcquired;
}

BOOL PLSingleLock::Unlock(LONG lCount, LPLONG lpPrevCount /* = NULL */)
{
	if (m_bAcquired)
		m_bAcquired = !m_pObject->Unlock(lCount, lpPrevCount);

	// successfully unlocking means it isn't acquired
	return !m_bAcquired;
}

PLSyncObject::PLSyncObject(LPCTSTR /*pstrName*/)
{
	m_hObject = NULL;
}

PLSyncObject::~PLSyncObject()
{
	if (m_hObject != NULL)
	{
		::CloseHandle(m_hObject);
		m_hObject = NULL;
	}
}

BOOL PLSyncObject::Lock(DWORD dwTimeout)
{
	if (::WaitForSingleObject(m_hObject, dwTimeout) == WAIT_OBJECT_0)
		return TRUE;
	else
		return FALSE;
}

#else // !_WIN32 (Linux)

// ---- PLSyncObject ----

PLSyncObject::PLSyncObject(LPCTSTR /*pstrName*/)
{
	m_hObject = NULL;
}

PLSyncObject::~PLSyncObject()
{
}

BOOL PLSyncObject::Lock(DWORD /*dwTimeout*/)
{
	return FALSE;
}

// ---- PLEvent ----

PLEvent::PLEvent(BOOL bInitiallyOwn, BOOL bManualReset, LPCTSTR /*pstrName*/,
	LPSECURITY_ATTRIBUTES /*lpsaAttribute*/)
	: PLSyncObject(NULL)
	, m_signaled(bInitiallyOwn ? true : false)
	, m_manualReset(bManualReset ? true : false)
{
	pthread_mutex_init(&m_eventMutex, NULL);
	pthread_cond_init(&m_eventCond, NULL);
	m_hObject = (HANDLE)this;
}

PLEvent::~PLEvent()
{
	pthread_cond_destroy(&m_eventCond);
	pthread_mutex_destroy(&m_eventMutex);
}

BOOL PLEvent::PulseEvent()
{
	pthread_mutex_lock(&m_eventMutex);
	m_signaled = true;
	pthread_cond_broadcast(&m_eventCond);
	m_signaled = false;
	pthread_mutex_unlock(&m_eventMutex);
	return TRUE;
}

BOOL PLEvent::Unlock()
{
	return SetEvent();
}

// ---- PLSemaphore ----

PLSemaphore::PLSemaphore(LONG lInitialCount, LONG lMaxCount,
	LPCTSTR /*pstrName*/, LPSECURITY_ATTRIBUTES /*lpsaAttributes*/)
	: PLSyncObject(NULL)
{
	sem_init(&m_sem, 0, (unsigned int)lInitialCount);
	(void)lMaxCount;
	m_hObject = (HANDLE)this;
}

PLSemaphore::~PLSemaphore()
{
	sem_destroy(&m_sem);
}

BOOL PLSemaphore::Unlock(LONG lCount, LPLONG /*lpPrevCount*/)
{
	for (LONG i = 0; i < lCount; i++)
		sem_post(&m_sem);
	return TRUE;
}

// ---- PLSingleLock ----

PLSingleLock::PLSingleLock(PLSyncObject* pObject, BOOL bInitialLock)
{
	m_pObject = pObject;
	m_hObject = pObject->m_hObject;
	m_bAcquired = FALSE;

	if (bInitialLock)
		Lock();
}

BOOL PLSingleLock::Lock(DWORD dwTimeOut)
{
	assert(!m_bAcquired);
	// Event-based wait is handled at PLEvent level
	m_bAcquired = m_pObject->Lock(dwTimeOut);
	return m_bAcquired;
}

BOOL PLSingleLock::Unlock()
{
	if (m_bAcquired)
		m_bAcquired = !m_pObject->Unlock();
	return !m_bAcquired;
}

BOOL PLSingleLock::Unlock(LONG lCount, LPLONG lpPrevCount)
{
	if (m_bAcquired)
		m_bAcquired = !m_pObject->Unlock(lCount, lpPrevCount);
	return !m_bAcquired;
}

#endif // _WIN32
