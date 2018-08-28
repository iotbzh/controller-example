/*
* Copyright (C) 2016 "IoT.bzh"
* Author Fulup Ar Foll <fulup@iot.bzh>
* Author Romain Forlot <romain@iot.bzh>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ctrl.h"

#define CONTROL_PREFIX "ctrl"
// default api to print log when apihandle not avaliable
afb_api_t AFB_default;

static CtlConfigT *CtrlLoadConfigJson(AFB_ApiT apiHandle, json_object *configJ);
static CtlConfigT *CtrlLoadConfigFile(AFB_ApiT apiHandle, const char *configPath);
static int CtrlCreateApi(AFB_ApiT apiHandle, CtlConfigT *ctrlConfig);

// Config Section definition
static CtlSectionT ctrlSections[] = {
	{.key = "plugin", .loadCB = PluginConfig},
	{.key = "control", .loadCB = ControlConfig},
	{.key = "events", .loadCB = EventConfig},
	{.key = "onload", .loadCB = OnloadConfig},
	{.key = NULL}
};

static void ctrlapi_ping(AFB_ReqT request) {
	static int count = 0;

	count++;
	AFB_ReqNotice(request, "Controller:ping count=%d", count);
	AFB_ReqSuccess(request, json_object_new_int(count), NULL);
}

static void ctrlapi_exit(AFB_ReqT request) {
	AFB_ReqNotice(request, "Exiting...");
	AFB_ReqSuccess(request, NULL, NULL);
	exit(0);
}

static AFB_ApiVerbs CtrlApiVerbs[] = {
	/* VERB'S NAME         FUNCTION TO CALL         SHORT DESCRIPTION */
	{.verb = "ping", .callback = ctrlapi_ping, .info = "ping test for API"},
	{.verb = "exit", .callback = ctrlapi_exit, .info = "Exit test"},
	{.verb = NULL} /* marker for end of the array */
};

static int CtrlLoadStaticVerbs(afb_api_t apiHandle, AFB_ApiVerbs *verbs) {
	int errcount = 0;

	for(int idx = 0; verbs[idx].verb; idx++) {
		errcount += afb_api_add_verb(
				apiHandle, CtrlApiVerbs[idx].verb, NULL, CtrlApiVerbs[idx].callback,
				(void *)&CtrlApiVerbs[idx], CtrlApiVerbs[idx].auth, 0, 0);
	}

	return errcount;
};

static int CtrlInitOneApi(AFB_ApiT apiHandle) {
	// Hugely hack to make all V2 AFB_DEBUG to work in fileutils
	AFB_default = apiHandle;

	CtlConfigT *ctrlConfig = afb_api_get_userdata(apiHandle);

	return CtlConfigExec(apiHandle, ctrlConfig);
}

static int CtrlLoadOneApi(void *cbdata, AFB_ApiT apiHandle) {
	CtlConfigT *ctrlConfig = (CtlConfigT *)cbdata;

	// save closure as api's data context
	afb_api_set_userdata(apiHandle, ctrlConfig);

	// add static controls verbs
	int err = CtrlLoadStaticVerbs(apiHandle, CtrlApiVerbs);
	if(err) {
		AFB_ApiError(apiHandle, "CtrlLoadSection fail to register static V2 verbs");
		return ERROR;
	}

	// load section for corresponding API
	err = CtlLoadSections(apiHandle, ctrlConfig, ctrlSections);

	// declare an event event manager for this API;
	afb_api_on_event(apiHandle, CtrlDispatchApiEvent);

	// init API function (does not receive user closure ???
	afb_api_on_init(apiHandle, CtrlInitOneApi);

	afb_api_seal(apiHandle);
	return err;
}

static CtlConfigT *CtrlLoadConfigJson(AFB_ApiT apiHandle, json_object *configJ) {
	return CtlLoadMetaDataJson(apiHandle, configJ, CONTROL_PREFIX);
}

static CtlConfigT *CtrlLoadConfigFile(AFB_ApiT apiHandle, const char *configPath) {
	return CtlLoadMetaDataUsingPrefix(apiHandle, configPath, CONTROL_PREFIX);
}

static int CtrlCreateApi(AFB_ApiT apiHandle, CtlConfigT *ctrlConfig) {
	json_object *resourcesJ = NULL, *eventsJ = NULL;

	if(!ctrlConfig) {
		AFB_ApiError(apiHandle,
			"CtrlBindingDyn No valid control config file loaded.");
			return ERROR;
	}

	if(!ctrlConfig->api) {
		AFB_ApiError(apiHandle,
			"CtrlBindingDyn API Missing from metadata in:\n-- %s",
			json_object_get_string(ctrlConfig->configJ));
		return ERROR;
	}

	AFB_ApiNotice(apiHandle, "Controller API='%s' info='%s'", ctrlConfig->api,
			ctrlConfig->info);
	if(! afb_api_new_api(apiHandle, ctrlConfig->api, ctrlConfig->info, 1, CtrlLoadOneApi, ctrlConfig))
		return ERROR;

	return 0;
}

int afbBindingEntry(afb_api_t apiHandle) {
	size_t len = 0, bindingRootDirLen = 0;
	char *dirList, *ctrlTestRootDir, *path;
	const char *envDirList = NULL, *configPath = NULL, *bindingRootDir = NULL;
	json_object *settings = afb_api_settings(apiHandle), *bpath = NULL;
	AFB_default = apiHandle;

	AFB_ApiDebug(apiHandle, "Controller in afbBindingEntry");

	if(json_object_object_get_ex(settings, "binding-path", &bpath)) {
		ctrlTestRootDir = strdup(json_object_get_string(bpath));
		path = rindex(ctrlTestRootDir, '/');
		if(strlen(path) < 3)
			return ERROR;
		*++path = '.';
		*++path = '.';
		*++path = '\0';
	}
	else {
		ctrlTestRootDir = malloc(1);
		strcpy(ctrlTestRootDir, "");
	}

	envDirList = getEnvDirList(CONTROL_PREFIX, "CONFIG_PATH");

	bindingRootDir = GetBindingDirPath();
	bindingRootDirLen = strlen(bindingRootDir);

	if(envDirList) {
		len = strlen(CONTROL_CONFIG_PATH) + strlen(envDirList) + bindingRootDirLen + 3;
		dirList = malloc(len + 1);
		snprintf(dirList, len +1, "%s:%s:%s:%s", envDirList, ctrlTestRootDir, bindingRootDir, CONTROL_CONFIG_PATH);
	}
	else {
		len = strlen(CONTROL_CONFIG_PATH) + bindingRootDirLen + 2;
		dirList = malloc(len + 1);
		snprintf(dirList, len + 1, "%s:%s:%s", bindingRootDir, ctrlTestRootDir, CONTROL_CONFIG_PATH);
	}

	configPath = CtlConfigSearch(apiHandle, dirList, CONTROL_PREFIX);
	if(!configPath) {
		AFB_ApiError(apiHandle, "CtlPreInit: No %s* config found in %s ", GetBinderName(), dirList);
		return ERROR;
	}

	free(ctrlTestRootDir);
	return CtrlCreateApi(apiHandle, CtrlLoadConfigFile(apiHandle, configPath));
}
