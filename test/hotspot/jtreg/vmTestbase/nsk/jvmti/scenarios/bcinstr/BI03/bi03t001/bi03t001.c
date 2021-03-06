/*
 * Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdlib.h>
#include <string.h>
#include "jni_tools.h"
#include "agent_common.h"
#include "jvmti_tools.h"

#define PASSED 0
#define STATUS_FAILED 2

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */

/* scaffold objects */
static jlong timeout = 0;

/* test objects */
static jclass debugeeClass = NULL;
static jbyteArray classBytes = NULL;
static int ClassFileLoadHookEventFlag = NSK_FALSE;

const char* CLASS_NAME = "nsk/jvmti/scenarios/bcinstr/BI03/bi03t001a";

/* ========================================================================== */

/** callback functions **/

static void JNICALL
ClassFileLoadHook(jvmtiEnv *jvmti_env, JNIEnv *jni_env,
        jclass class_being_redefined, jobject loader,
        const char* name, jobject protection_domain,
        jint class_data_len, const unsigned char* class_data,
        jint *new_class_data_len, unsigned char** new_class_data) {

    if (name != NULL && (strcmp(name, CLASS_NAME) == 0)) {
        ClassFileLoadHookEventFlag = NSK_TRUE;
        NSK_DISPLAY0("ClassFileLoadHook event\n");

        if (class_being_redefined == NULL) {
            /* sent by class load */

            if (!NSK_JNI_VERIFY(jni_env, (*new_class_data_len =
                    NSK_CPP_STUB2(GetArrayLength, jni_env, classBytes)) > 0)) {
                nsk_jvmti_setFailStatus();
                return;
            }

            if (!NSK_JNI_VERIFY(jni_env, (*new_class_data = (unsigned char*)
                    NSK_CPP_STUB3(GetByteArrayElements, jni_env, classBytes, NULL))
                        != NULL)) {
                nsk_jvmti_setFailStatus();
                return;
            }
        }
    }
}

/* ========================================================================== */

static int prepare(jvmtiEnv* jvmti, JNIEnv* jni) {
    const char* DEBUGEE_CLASS_NAME =
        "nsk/jvmti/scenarios/bcinstr/BI03/bi03t001";
    jfieldID field = NULL;

    NSK_DISPLAY1("Find class: %s\n", DEBUGEE_CLASS_NAME);
    if (!NSK_JNI_VERIFY(jni, (debugeeClass =
            NSK_CPP_STUB2(FindClass, jni, DEBUGEE_CLASS_NAME)) != NULL))
        return NSK_FALSE;

    if (!NSK_JNI_VERIFY(jni, (debugeeClass =
            NSK_CPP_STUB2(NewGlobalRef, jni, debugeeClass)) != NULL))
        return NSK_FALSE;

    if (!NSK_JNI_VERIFY(jni, (field =
            NSK_CPP_STUB4(GetStaticFieldID, jni, debugeeClass,
                "newClassBytes", "[B")) != NULL))
        return NSK_FALSE;

    if (!NSK_JNI_VERIFY(jni, (classBytes = (jbyteArray)
            NSK_CPP_STUB3(GetStaticObjectField, jni, debugeeClass, field))
                != NULL))
        return NSK_FALSE;

    if (!NSK_JNI_VERIFY(jni, (classBytes =
            NSK_CPP_STUB2(NewGlobalRef, jni, classBytes)) != NULL))
        return NSK_FALSE;

    if (!NSK_JVMTI_VERIFY(NSK_CPP_STUB4(SetEventNotificationMode,
            jvmti, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL)))
        return JNI_ERR;

    return NSK_TRUE;
}

/* ========================================================================== */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {

    if (!nsk_jvmti_waitForSync(timeout))
        return;

    if (!prepare(jvmti, jni)) {
        nsk_jvmti_setFailStatus();
        return;
    }

    /* resume debugee and wait for sync */
    if (!nsk_jvmti_resumeSync())
        return;
    if (!nsk_jvmti_waitForSync(timeout))
        return;

    if (!NSK_VERIFY(ClassFileLoadHookEventFlag)) {
        NSK_COMPLAIN0("Missing ClassFileLoadHook event\n");
        nsk_jvmti_setFailStatus();
    }

    if (!NSK_JVMTI_VERIFY(NSK_CPP_STUB4(SetEventNotificationMode,
            jvmti, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL)))
        nsk_jvmti_setFailStatus();

    NSK_TRACE(NSK_CPP_STUB2(DeleteGlobalRef, jni, debugeeClass));
    NSK_TRACE(NSK_CPP_STUB2(DeleteGlobalRef, jni, classBytes));

    if (!nsk_jvmti_resumeSync())
        return;
}

/* ========================================================================== */

/** Agent library initialization. */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_bi03t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_bi03t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_bi03t001(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv* jvmti = NULL;
    jvmtiEventCallbacks callbacks;
    jvmtiCapabilities caps;

    NSK_DISPLAY0("Agent_OnLoad\n");

    if (!NSK_VERIFY(nsk_jvmti_parseOptions(options)))
        return JNI_ERR;

    timeout = nsk_jvmti_getWaitTime() * 60 * 1000;

    if (!NSK_VERIFY((jvmti =
            nsk_jvmti_createJVMTIEnv(jvm, reserved)) != NULL))
        return JNI_ERR;

    memset(&caps, 0, sizeof(caps));
    caps.can_generate_all_class_hook_events = 1;
    if (!NSK_JVMTI_VERIFY(NSK_CPP_STUB2(AddCapabilities, jvmti, &caps)))
        return JNI_ERR;

    if (!NSK_VERIFY(nsk_jvmti_setAgentProc(agentProc, NULL)))
        return JNI_ERR;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = &ClassFileLoadHook;
    if (!NSK_JVMTI_VERIFY(NSK_CPP_STUB3(SetEventCallbacks,
            jvmti, &callbacks, sizeof(callbacks))))
        return JNI_ERR;

    return JNI_OK;
}

/* ========================================================================== */

#ifdef __cplusplus
}
#endif
