//
//  CpuTscSync.cpp
//  CpuTscSync
//
//  Copyright © 2020 lvs1974. All rights reserved.
//

#include <Headers/kern_api.hpp>

#include "CpuTscSync.hpp"
#include "VoodooTSCSync.h"

static CpuTscSyncPlugin *callbackCpuf = nullptr;

void CpuTscSyncPlugin::init()
{
	callbackCpuf = this;
	
	lilu.onPatcherLoadForce(
		[](void *user, KernelPatcher &patcher) {
			static_cast<CpuTscSyncPlugin *>(user)->processKernel(patcher);
		}, this);
}

void CpuTscSyncPlugin::xcpm_urgency(int urgency, uint64_t rt_period, uint64_t rt_deadline)
{
	if (!VoodooTSCSync::isTscSynced())
	{
		SYSLOG("cputs", "xcpm_urgency is called when TSC presumably is not in sync, sync it");
        return;
	}
	
	FunctionCast(xcpm_urgency, callbackCpuf->org_xcpm_urgency)(urgency, rt_period, rt_deadline);
}

void CpuTscSyncPlugin::willEnterFullWake(void *pmRoot) {
    mp_rendezvous_no_intrs(reset_tsc_adjust, NULL);
    
    FunctionCast(willEnterFullWake, callbackCpuf->orgWillEnterFullWake)(pmRoot);
}

void CpuTscSyncPlugin::processKernel(KernelPatcher &patcher)
{
	if (!kernel_routed)
	{
        KernelPatcher::RouteRequest requests[] {
            {"__ZN14IOPMrootDomain17willEnterFullWakeEv", willEnterFullWake, orgWillEnterFullWake},
            {"_xcpm_urgency", xcpm_urgency, org_xcpm_urgency}
        };
        
		if (!patcher.routeMultipleLong(KernelPatcher::KernelID, requests))
			SYSLOG("cputs", "patcher.routeMultiple for %s is failed with error %d", requests[0].symbol, patcher.getError());
		kernel_routed = true;
	}

	// Ignore all the errors for other processors
	patcher.clearError();
}
