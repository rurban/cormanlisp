//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ThreadClasses.cpp
//		Contents:	Thread synchronization classes for Corman Lisp.
//		            Linux port: Win32 primitives replaced with pthreads.
//		History:	8/5/97  RGC Created.
//

#include "Stdafx.h"
#include "ThreadClasses.h"
#include <assert.h>

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
