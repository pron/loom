/*
 * Copyright (c) 2003, 2022, Oracle and/or its affiliates. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "jvmti.h"
#include "jvmti_common.h"

extern "C" {


#define PASSED 0
#define STATUS_FAILED 2

typedef struct {
  const char *cls_sig;
  const char *name;
  const char *sig;
  jlocation loc;
} method_location_info;

static jvmtiEnv *jvmti = NULL;
static jvmtiEventCallbacks callbacks;
static jint result = PASSED;
static jboolean isVirtualExpected = JNI_FALSE;
static size_t eventsExpected = 0;
static size_t eventsCount = 0;
static method_location_info exits[] = {
    { "Lmexit01a;", "chain", "()V", -1 },
    { "Lmexit01a;", "dummy", "()V", 3 }
};

void JNICALL MethodExit(jvmtiEnv *jvmti, JNIEnv *jni,
                        jthread thread, jmethodID method,
                        jboolean was_poped_by_exc, jvalue return_value) {
  jvmtiError err;
  char *cls_sig, *name, *sig, *generic;
  jclass cls;
  jmethodID mid;
  jlocation loc;
  char buffer[32];

  err = jvmti->GetMethodDeclaringClass(method, &cls);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(GetMethodDeclaringClass) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }
  err = jvmti->GetClassSignature(cls, &cls_sig, &generic);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(GetClassSignature) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }
  if (cls_sig != NULL && strcmp(cls_sig, "Lmexit01a;") == 0) {

    LOG(">>> retrieving method exit info ...\n");
    err = jvmti->GetMethodName(method, &name, &sig, &generic);
    if (err != JVMTI_ERROR_NONE) {
      LOG("(GetMethodName) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
      return;
    }
    err = jvmti->GetFrameLocation(thread, 0, &mid, &loc);
    if (err != JVMTI_ERROR_NONE) {
      LOG("(GetFrameLocation) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
      return;
    }

    LOG(">>>      class: \"%s\"\n", cls_sig);
    LOG(">>>     method: \"%s%s\"\n", name, sig);
    LOG(">>>   location: %s\n", jlong_to_string(loc, buffer));
    LOG(">>> ... done\n");

    if (eventsCount < sizeof(exits) / sizeof(method_location_info)) {
      if (cls_sig == NULL || strcmp(cls_sig, exits[eventsCount].cls_sig) != 0) {
        LOG("(exit#%" PRIuPTR ") wrong class: \"%s\"", eventsCount, cls_sig);
        LOG(", expected: \"%s\"\n", exits[eventsCount].cls_sig);
        result = STATUS_FAILED;
      }
      if (name == NULL || strcmp(name, exits[eventsCount].name) != 0) {
        LOG("(exit#%" PRIuPTR ") wrong method name: \"%s\"", eventsCount, name);
        LOG(", expected: \"%s\"\n", exits[eventsCount].name);
        result = STATUS_FAILED;
      }
      if (sig == NULL || strcmp(sig, exits[eventsCount].sig) != 0) {
        LOG("(exit#%" PRIuPTR ") wrong method sig: \"%s\"", eventsCount, sig);
        LOG(", expected: \"%s\"\n", exits[eventsCount].sig);
        result = STATUS_FAILED;
      }
      if (loc != exits[eventsCount].loc) {
        LOG("(exit#%" PRIuPTR ") wrong location: %s", eventsCount, jlong_to_string(loc, buffer));
        LOG(", expected: %s\n", jlong_to_string(exits[eventsCount].loc, buffer));
        result = STATUS_FAILED;
      }
      jboolean isVirtual = jni->IsVirtualThread(thread);
      if (isVirtualExpected != isVirtual) {
        LOG("The thread IsVirtualThread %d differs from expected %d.\n", isVirtual, isVirtualExpected);
        result = STATUS_FAILED;
      }
    } else {
      LOG("Unexpected method exit catched:");
      LOG("     class: \"%s\"\n", cls_sig);
      LOG("    method: \"%s%s\"\n", name, sig);
      LOG("  location: %s\n", jlong_to_string(loc, buffer));
      result = STATUS_FAILED;
    }
    eventsCount++;
  }
}

#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_mexit01(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_mexit01(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_mexit01(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
  jvmtiCapabilities caps;
  jvmtiError err;
  jint res;

  res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    LOG("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  err = jvmti->GetPotentialCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(GetPotentialCapabilities) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }

  err = jvmti->AddCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(AddCapabilities) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }

  err = jvmti->GetCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(GetCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  if (caps.can_generate_method_exit_events) {
    callbacks.MethodExit = &MethodExit;
    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
      LOG("(SetEventCallbacks) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      return JNI_ERR;
    }
  } else {
    LOG("Warning: MethodExit event is not implemented\n");
  }

  return JNI_OK;
}

JNIEXPORT jint JNICALL
Java_mexit01_init0(JNIEnv *jni, jclass cls) {
  jvmtiError err;
  if (jvmti == NULL) {
    LOG("JVMTI client was not properly loaded!\n");
    return STATUS_FAILED;
  }

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_METHOD_EXIT, NULL);
  if (err == JVMTI_ERROR_NONE) {
    eventsExpected = sizeof(exits)/sizeof(method_location_info);
    eventsCount = 0;
  } else {
    LOG("Failed to enable JVMTI_EVENT_METHOD_EXIT event: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }

  // TODO: should we return result instead?
  return PASSED;
}

JNIEXPORT jint JNICALL
Java_mexit01_check(JNIEnv *jni, jclass cls) {
  jvmtiError err;
  jclass clz;
  jmethodID mid;

  if (jvmti == NULL) {
    LOG("JVMTI client was not properly loaded!\n");
    return STATUS_FAILED;
  }

  jthread thread;
  err = jvmti->GetCurrentThread(&thread);
  if (err != JVMTI_ERROR_NONE) {
    LOG("Failed to get current thread: %s (%d)\n", TranslateError(err), err);
    result = STATUS_FAILED;
  }
  isVirtualExpected = jni->IsVirtualThread(thread);

  clz = jni->FindClass("mexit01a");
  if (clz == NULL) {
    LOG("Cannot find MethodExit.mexit01a class!\n");
    return STATUS_FAILED;
  }

  mid = jni->GetStaticMethodID(clz, "dummy", "()V");
  if (mid == NULL) {
    LOG("Cannot find metod \"dummy()\"!\n");
    return STATUS_FAILED;
  }

  jni->CallStaticVoidMethod(clz, mid);

  err = jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                        JVMTI_EVENT_METHOD_EXIT, NULL);
  if (err != JVMTI_ERROR_NONE) {
    LOG("Failed to disable JVMTI_EVENT_METHOD_EXIT event: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }

  if (eventsCount != eventsExpected) {
    LOG("Wrong number of MethodExit events: %" PRIuPTR ", expected: %" PRIuPTR "\n", eventsCount, eventsExpected);
    result = STATUS_FAILED;
  }
  return result;
}

JNIEXPORT void JNICALL
Java_mexit01a_chain(JNIEnv *jni, jclass cls) {
  LOG("Executing chain()\n");
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  return Agent_Initialize(jvm, options, reserved);
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *jvm, char *options, void *reserved) {
  return Agent_Initialize(jvm, options, reserved);
}



}
