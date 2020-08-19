# Copyright (C) 2013 Samsung Electronics. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest

from webkitcorepy import string_utils, OutputCapture

from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.port.leakdetector_valgrind import LeakDetectorValgrind


def make_mock_valgrind_output(process_name, pid, uuid):
    return """<?xml version="1.0"?>

<valgrindoutput>

<protocolversion>4</protocolversion>
<protocoltool>memcheck</protocoltool>

<preamble>
  <line>Memcheck, a memory error detector</line>
  <line>Copyright (C) 2002-2011, and GNU GPL'd, by Julian Seward et al.</line>
  <line>Using Valgrind-3.7.0 and LibVEX; rerun with -h for copyright info</line>
  <line>Command: /home/user/WebKit/WebKitBuild/Release/Programs/{process_name} -</line>
</preamble>

<pid>{pid}</pid>
<ppid>18577</ppid>
<tool>memcheck</tool>

<args>
  <vargv>
    <exe>/usr/bin/valgrind.bin</exe>
    <arg>--tool=memcheck</arg>
    <arg>--num-callers=40</arg>
    <arg>--demangle=no</arg>
    <arg>--trace-children=no</arg>
    <arg>--smc-check=all-non-file</arg>
    <arg>--leak-check=yes</arg>
    <arg>--leak-resolution=high</arg>
    <arg>--show-possibly-lost=no</arg>
    <arg>--show-reachable=no</arg>
    <arg>--leak-check=full</arg>
    <arg>--undef-value-errors=no</arg>
    <arg>--gen-suppressions=all</arg>
    <arg>--xml=yes</arg>
    <arg>--xml-file=/home/user/WebKit/WebKitBuild/Release/layout-test-results/drt-{pid}-{uuid}-leaks.xml</arg>
    <arg>--suppressions=/home/user/WebKit/Tools/Scripts/valgrind/suppressions.txt</arg>
    <arg>--suppressions=/usr/lib/valgrind/debian-libc6-dbg.supp</arg>
  </vargv>
  <argv>
    <exe>/home/user/WebKit/WebKitBuild/Release/Programs/{process_name}</exe>
    <arg>-</arg>
  </argv>
</args>

<status>
  <state>RUNNING</state>
  <time>00:00:00:00.024 </time>
</status>


<status>
  <state>FINISHED</state>
  <time>00:00:00:54.186 </time>
</status>

<error>
  <unique>0x1a4</unique>
  <tid>1</tid>
  <kind>Leak_DefinitelyLost</kind>
  <xwhat>
    <text>8 bytes in 1 blocks are definitely lost in loss record 421 of 7,972</text>
    <leakedbytes>8</leakedbytes>
    <leakedblocks>1</leakedblocks>
  </xwhat>
  <stack>
    <frame>
      <ip>0x4C2AF8E</ip>
      <obj>/usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so</obj>
      <fn>_Znwm</fn>
    </frame>
    <frame>
      <ip>0x6839DEC</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation</fn>
    </frame>
    <frame>
      <ip>0x61E7B03</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>webkit_web_policy_decision_new</fn>
    </frame>
    <frame>
      <ip>0x61CBA6D</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE</fn>
    </frame>
    <frame>
      <ip>0x683DF52</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_</fn>
    </frame>
    <frame>
      <ip>0x6817EFC</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE</fn>
    </frame>
    <frame>
      <ip>0x6818729</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE</fn>
    </frame>
    <frame>
      <ip>0x6818A3A</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE</fn>
    </frame>
    <frame>
      <ip>0x61E3148</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>webkit_web_frame_load_uri</fn>
    </frame>
    <frame>
      <ip>0x44CBC9</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL7runTestRKSs</fn>
    </frame>
    <frame>
      <ip>0x44CED6</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL20runTestingServerLoopv</fn>
    </frame>
    <frame>
      <ip>0x43A2D3</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>main</fn>
    </frame>
  </stack>
  <suppression>
    <sname>insert_a_suppression_name_here</sname>
    <skind>Memcheck:Leak</skind>
    <sframe> <fun>_Znwm</fun> </sframe>
    <sframe> <fun>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation</fun> </sframe>
    <sframe> <fun>webkit_web_policy_decision_new</fun> </sframe>
    <sframe> <fun>_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE</fun> </sframe>
    <sframe> <fun>webkit_web_frame_load_uri</fun> </sframe>
    <sframe> <fun>_ZL7runTestRKSs</fun> </sframe>
    <sframe> <fun>_ZL20runTestingServerLoopv</fun> </sframe>
    <sframe> <fun>main</fun> </sframe>
    <rawtext>
<![CDATA[
{{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znwm
   fun:_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation
   fun:webkit_web_policy_decision_new
   fun:_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE
   fun:_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_
   fun:_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE
   fun:_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE
   fun:_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE
   fun:webkit_web_frame_load_uri
   fun:_ZL7runTestRKSs
   fun:_ZL20runTestingServerLoopv
   fun:main
}}
]]>
    </rawtext>
  </suppression>
</error>

<error>
  <unique>0x1a5</unique>
  <tid>1</tid>
  <kind>Leak_DefinitelyLost</kind>
  <xwhat>
    <text>8 bytes in 1 blocks are definitely lost in loss record 422 of 7,972</text>
    <leakedbytes>8</leakedbytes>
    <leakedblocks>1</leakedblocks>
  </xwhat>
  <stack>
    <frame>
      <ip>0x4C2AF8E</ip>
      <obj>/usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so</obj>
      <fn>_Znwm</fn>
    </frame>
    <frame>
      <ip>0x6839D0C</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker18checkContentPolicyERKNS1_16ResourceResponseEPFvPvNS1_12PolicyActionEES6_EUlS7_E_E10_M_managerERSt9_Any_dataRKSC_St18_Manager_operation</fn>
    </frame>
    <frame>
      <ip>0x61E7B03</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>webkit_web_policy_decision_new</fn>
    </frame>
    <frame>
      <ip>0x61CB527</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN6WebKit17FrameLoaderClient31dispatchDecidePolicyForResponseERKN7WebCore16ResourceResponseERKNS1_15ResourceRequestESt8functionIFvNS1_12PolicyActionEEE</fn>
    </frame>
    <frame>
      <ip>0x6839EC0</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore13PolicyChecker18checkContentPolicyERKNS_16ResourceResponseEPFvPvNS_12PolicyActionEES4_</fn>
    </frame>
    <frame>
      <ip>0x6802F6E</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore14DocumentLoader16responseReceivedEPNS_14CachedResourceERKNS_16ResourceResponseE</fn>
    </frame>
    <frame>
      <ip>0x67E59E0</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore17CachedRawResource16responseReceivedERKNS_16ResourceResponseE</fn>
    </frame>
    <frame>
      <ip>0x684C237</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore17SubresourceLoader18didReceiveResponseERKNS_16ResourceResponseE</fn>
    </frame>
    <frame>
      <ip>0x6F95848</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCoreL19sendRequestCallbackEP8_GObjectP13_GAsyncResultPv</fn>
    </frame>
    <frame>
      <ip>0x8E2D6CA</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libgio-2.0.so.0.3800.0</obj>
      <fn>g_task_return_now</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/gio</dir>
      <file>gtask.c</file>
      <line>1108</line>
    </frame>
    <frame>
      <ip>0x8E2D6E8</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libgio-2.0.so.0.3800.0</obj>
      <fn>complete_in_idle_cb</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/gio</dir>
      <file>gtask.c</file>
      <line>1117</line>
    </frame>
    <frame>
      <ip>0x93A62F4</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libglib-2.0.so.0.3800.0</obj>
      <fn>g_main_context_dispatch</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib</dir>
      <file>gmain.c</file>
      <line>3065</line>
    </frame>
    <frame>
      <ip>0x93A6637</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libglib-2.0.so.0.3800.0</obj>
      <fn>g_main_context_iterate.isra.23</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib</dir>
      <file>gmain.c</file>
      <line>3712</line>
    </frame>
    <frame>
      <ip>0x93A6A99</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libglib-2.0.so.0.3800.0</obj>
      <fn>g_main_loop_run</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib</dir>
      <file>gmain.c</file>
      <line>3906</line>
    </frame>
    <frame>
      <ip>0x8121204</ip>
      <obj>/home/user/WebKit/WebKitBuild/Dependencies/Root/lib64/libgtk-3.so.0.600.0</obj>
      <fn>gtk_main</fn>
      <dir>/home/user/WebKit/WebKitBuild/Dependencies/Source/gtk+-3.6.0/gtk</dir>
      <file>gtkmain.c</file>
      <line>1162</line>
    </frame>
    <frame>
      <ip>0x44CBCE</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL7runTestRKSs</fn>
    </frame>
    <frame>
      <ip>0x44CED6</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL20runTestingServerLoopv</fn>
    </frame>
    <frame>
      <ip>0x43A2D3</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>main</fn>
    </frame>
  </stack>
  <suppression>
    <sname>insert_a_suppression_name_here</sname>
    <skind>Memcheck:Leak</skind>
    <sframe> <fun>_Znwm</fun> </sframe>
    <sframe> <fun>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker18checkContentPolicyERKNS1_16ResourceResponseEPFvPvNS1_12PolicyActionEES6_EUlS7_E_E10_M_managerERSt9_Any_dataRKSC_St18_Manager_operation</fun> </sframe>
    <sframe> <fun>webkit_web_policy_decision_new</fun> </sframe>
    <sframe> <fun>_ZN6WebKit17FrameLoaderClient31dispatchDecidePolicyForResponseERKN7WebCore16ResourceResponseERKNS1_15ResourceRequestESt8functionIFvNS1_12PolicyActionEEE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore13PolicyChecker18checkContentPolicyERKNS_16ResourceResponseEPFvPvNS_12PolicyActionEES4_</fun> </sframe>
    <sframe> <fun>_ZN7WebCore14DocumentLoader16responseReceivedEPNS_14CachedResourceERKNS_16ResourceResponseE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore17CachedRawResource16responseReceivedERKNS_16ResourceResponseE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore17SubresourceLoader18didReceiveResponseERKNS_16ResourceResponseE</fun> </sframe>
    <sframe> <fun>_ZN7WebCoreL19sendRequestCallbackEP8_GObjectP13_GAsyncResultPv</fun> </sframe>
    <sframe> <fun>g_task_return_now</fun> </sframe>
    <sframe> <fun>complete_in_idle_cb</fun> </sframe>
    <sframe> <fun>g_main_context_dispatch</fun> </sframe>
    <sframe> <fun>g_main_context_iterate.isra.23</fun> </sframe>
    <sframe> <fun>g_main_loop_run</fun> </sframe>
    <sframe> <fun>gtk_main</fun> </sframe>
    <sframe> <fun>_ZL7runTestRKSs</fun> </sframe>
    <sframe> <fun>_ZL20runTestingServerLoopv</fun> </sframe>
    <sframe> <fun>main</fun> </sframe>
    <rawtext>
<![CDATA[
{{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znwm
   fun:_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker18checkContentPolicyERKNS1_16ResourceResponseEPFvPvNS1_12PolicyActionEES6_EUlS7_E_E10_M_managerERSt9_Any_dataRKSC_St18_Manager_operation
   fun:webkit_web_policy_decision_new
   fun:_ZN6WebKit17FrameLoaderClient31dispatchDecidePolicyForResponseERKN7WebCore16ResourceResponseERKNS1_15ResourceRequestESt8functionIFvNS1_12PolicyActionEEE
   fun:_ZN7WebCore13PolicyChecker18checkContentPolicyERKNS_16ResourceResponseEPFvPvNS_12PolicyActionEES4_
   fun:_ZN7WebCore14DocumentLoader16responseReceivedEPNS_14CachedResourceERKNS_16ResourceResponseE
   fun:_ZN7WebCore17CachedRawResource16responseReceivedERKNS_16ResourceResponseE
   fun:_ZN7WebCore17SubresourceLoader18didReceiveResponseERKNS_16ResourceResponseE
   fun:_ZN7WebCoreL19sendRequestCallbackEP8_GObjectP13_GAsyncResultPv
   fun:g_task_return_now
   fun:complete_in_idle_cb
   fun:g_main_context_dispatch
   fun:g_main_context_iterate.isra.23
   fun:g_main_loop_run
   fun:gtk_main
   fun:_ZL7runTestRKSs
   fun:_ZL20runTestingServerLoopv
   fun:main
}}
]]>
    </rawtext>
  </suppression>
</error>

<error>
  <unique>0x1a6</unique>
  <tid>1</tid>
  <kind>Leak_DefinitelyLost</kind>
  <xwhat>
    <text>8 bytes in 1 blocks are definitely lost in loss record 423 of 7,972</text>
    <leakedbytes>8</leakedbytes>
    <leakedblocks>1</leakedblocks>
  </xwhat>
  <stack>
    <frame>
      <ip>0x4C2AF8E</ip>
      <obj>/usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so</obj>
      <fn>_Znwm</fn>
    </frame>
    <frame>
      <ip>0x6839DEC</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation</fn>
    </frame>
    <frame>
      <ip>0x61E7B03</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>webkit_web_policy_decision_new</fn>
    </frame>
    <frame>
      <ip>0x61CBA6D</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE</fn>
    </frame>
    <frame>
      <ip>0x683DF52</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_</fn>
    </frame>
    <frame>
      <ip>0x6817EFC</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE</fn>
    </frame>
    <frame>
      <ip>0x6818729</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE</fn>
    </frame>
    <frame>
      <ip>0x6818A3A</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE</fn>
    </frame>
    <frame>
      <ip>0x61E3148</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3</obj>
      <fn>webkit_web_frame_load_uri</fn>
    </frame>
    <frame>
      <ip>0x44CC50</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL7runTestRKSs</fn>
    </frame>
    <frame>
      <ip>0x44CED6</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>_ZL20runTestingServerLoopv</fn>
    </frame>
    <frame>
      <ip>0x43A2D3</ip>
      <obj>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</obj>
      <fn>main</fn>
    </frame>
  </stack>
  <suppression>
    <sname>insert_a_suppression_name_here</sname>
    <skind>Memcheck:Leak</skind>
    <sframe> <fun>_Znwm</fun> </sframe>
    <sframe> <fun>_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation</fun> </sframe>
    <sframe> <fun>webkit_web_policy_decision_new</fun> </sframe>
    <sframe> <fun>_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE</fun> </sframe>
    <sframe> <fun>_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE</fun> </sframe>
    <sframe> <fun>webkit_web_frame_load_uri</fun> </sframe>
    <sframe> <fun>_ZL7runTestRKSs</fun> </sframe>
    <sframe> <fun>_ZL20runTestingServerLoopv</fun> </sframe>
    <sframe> <fun>main</fun> </sframe>
    <rawtext>
<![CDATA[
{{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znwm
   fun:_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation
   fun:webkit_web_policy_decision_new
   fun:_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE
   fun:_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_
   fun:_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE
   fun:_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE
   fun:_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE
   fun:webkit_web_frame_load_uri
   fun:_ZL7runTestRKSs
   fun:_ZL20runTestingServerLoopv
   fun:main
}}
]]>
    </rawtext>
  </suppression>
</error>

<errorcounts>
</errorcounts>

<suppcounts>
  <pair>
    <count>107</count>
    <name>FcConfigAppFontAddFile (Third Party)</name>
  </pair>
  <pair>
    <count>2098</count>
    <name>gtk_init_check (Third Party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>g_quark_from_static_string (Third party)</name>
  </pair>
  <pair>
    <count>27</count>
    <name>FcConfigParseAndLoad (Third Party)</name>
  </pair>
  <pair>
    <count>80</count>
    <name>webkitAccessibleNew</name>
  </pair>
  <pair>
    <count>177</count>
    <name>g_thread_proxy (Third Party)</name>
  </pair>
  <pair>
    <count>9</count>
    <name>FcPatternObjectInsertElt 2 (Third Party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>gtk_window_realize (Third Party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>__nss_database_lookup (Third Party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>cairo_set_source_surface (Third Party)</name>
  </pair>
  <pair>
    <count>2</count>
    <name>libGL.so (Third party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>g_task_run_in_thread (Third Party)</name>
  </pair>
  <pair>
    <count>2</count>
    <name>WTF::ThreadIdentifierData::initialize() (Intentional)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>gtk_css_provider_load_from_data (Third Party)</name>
  </pair>
  <pair>
    <count>1</count>
    <name>libenchant.so new (Third party)</name>
  </pair>
</suppcounts>

</valgrindoutput>
""".format(process_name=process_name, pid=pid, uuid=uuid)


def make_mock_incomplete_valgrind_output(process_name, pid, uuid):
    return """<?xml version="1.0"?>

<valgrindoutput>

<protocolversion>4</protocolversion>
<protocoltool>memcheck</protocoltool>

<preamble>
  <line>Memcheck, a memory error detector</line>
  <line>Copyright (C) 2002-2011, and GNU GPL'd, by Julian Seward et al.</line>
  <line>Using Valgrind-3.7.0 and LibVEX; rerun with -h for copyright info</line>
  <line>Command: /home/user/WebKit/WebKitBuild/Release/Programs/{process_name} -</line>
</preamble>

<pid>{pid}</pid>
<ppid>18577</ppid>
<tool>memcheck</tool>

<args>
  <vargv>
    <exe>/usr/bin/valgrind.bin</exe>
    <arg>--tool=memcheck</arg>
    <arg>--num-callers=40</arg>
    <arg>--demangle=no</arg>
    <arg>--trace-children=no</arg>
    <arg>--smc-check=all-non-file</arg>
    <arg>--leak-check=yes</arg>
    <arg>--leak-resolution=high</arg>
    <arg>--show-possibly-lost=no</arg>
    <arg>--show-reachable=no</arg>
    <arg>--leak-check=full</arg>
    <arg>--undef-value-errors=no</arg>
    <arg>--gen-suppressions=all</arg>
    <arg>--xml=yes</arg>
    <arg>--xml-file=/home/user/WebKit/WebKitBuild/Release/layout-test-results/drt-{pid}-{uuid}-leaks.xml</arg>
    <arg>--suppressions=/home/user/WebKit/Tools/Scripts/valgrind/suppressions.txt</arg>
    <arg>--suppressions=/usr/lib/valgrind/debian-libc6-dbg.supp</arg>
  </vargv>
  <argv>
    <exe>/home/user/WebKit/WebKitBuild/Release/Programs/{process_name}</exe>
    <arg>-</arg>
  </argv>
</args>

<status>
  <state>RUNNING</state>
  <time>00:00:00:00.024 </time>
</status>
""".format(process_name=process_name, pid=pid, uuid=uuid)


def make_mock_valgrind_results():
    return """-----------------------------------------------------
Suppressions used:
  count name
      2 __nss_database_lookup (Third Party)
      2 cairo_set_source_surface (Third Party)
      2 g_quark_from_static_string (Third party)
      2 g_task_run_in_thread (Third Party)
      2 gtk_css_provider_load_from_data (Third Party)
      2 gtk_window_realize (Third Party)
      2 libenchant.so new (Third party)
      4 WTF::ThreadIdentifierData::initialize() (Intentional)
      4 libGL.so (Third party)
     18 FcPatternObjectInsertElt 2 (Third Party)
     54 FcConfigParseAndLoad (Third Party)
    160 webkitAccessibleNew
    214 FcConfigAppFontAddFile (Third Party)
    354 g_thread_proxy (Third Party)
   4196 gtk_init_check (Third Party)
-----------------------------------------------------
Valgrind detected 2 leaks:
Leak_DefinitelyLost
8 bytes in 1 blocks are definitely lost in loss record 422 of 7,972
  operator new(unsigned long) (/usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
  std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*)::{lambda(WebCore::PolicyAction)#1}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*)::{lambda(WebCore::PolicyAction)#1}> const&, std::_Manager_operation) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  webkit_web_policy_decision_new (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebKit::FrameLoaderClient::dispatchDecidePolicyForResponse(WebCore::ResourceResponse const&, WebCore::ResourceRequest const&, std::function<void (WebCore::PolicyAction)>) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::DocumentLoader::responseReceived(WebCore::CachedResource*, WebCore::ResourceResponse const&) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::CachedRawResource::responseReceived(WebCore::ResourceResponse const&) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::SubresourceLoader::didReceiveResponse(WebCore::ResourceResponse const&) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::sendRequestCallback(_GObject*, _GAsyncResult*, void*) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  g_task_return_now (/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/gio/gtask.c:1108)
  complete_in_idle_cb (/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/gio/gtask.c:1117)
  g_main_context_dispatch (/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib/gmain.c:3065)
  g_main_context_iterate.isra.23 (/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib/gmain.c:3712)
  g_main_loop_run (/home/user/WebKit/WebKitBuild/Dependencies/Source/glib-2.38.0/glib/gmain.c:3906)
  gtk_main (/home/user/WebKit/WebKitBuild/Dependencies/Source/gtk+-3.6.0/gtk/gtkmain.c:1162)
  runTest(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
  runTestingServerLoop() (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
  main (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
Suppression (error hash=#8313DEB16B069438#):

{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znw*
   fun:_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker18checkContentPolicyERKNS1_16ResourceResponseEPFvPvNS1_12PolicyActionEES6_EUlS7_E_E10_M_managerERSt9_Any_dataRKSC_St18_Manager_operation
   fun:webkit_web_policy_decision_new
   fun:_ZN6WebKit17FrameLoaderClient31dispatchDecidePolicyForResponseERKN7WebCore16ResourceResponseERKNS1_15ResourceRequestESt8functionIFvNS1_12PolicyActionEEE
   fun:_ZN7WebCore13PolicyChecker18checkContentPolicyERKNS_16ResourceResponseEPFvPvNS_12PolicyActionEES4_
   fun:_ZN7WebCore14DocumentLoader16responseReceivedEPNS_14CachedResourceERKNS_16ResourceResponseE
   fun:_ZN7WebCore17CachedRawResource16responseReceivedERKNS_16ResourceResponseE
   fun:_ZN7WebCore17SubresourceLoader18didReceiveResponseERKNS_16ResourceResponseE
   fun:_ZN7WebCoreL19sendRequestCallbackEP8_GObjectP13_GAsyncResultPv
   fun:g_task_return_now
   fun:complete_in_idle_cb
   fun:g_main_context_dispatch
   fun:g_main_context_iterate.isra.23
   fun:g_main_loop_run
   fun:gtk_main
   fun:_ZL7runTestRKSs
   fun:_ZL20runTestingServerLoopv
   fun:main
}


Leak_DefinitelyLost
8 bytes in 1 blocks are definitely lost in loss record 421 of 7,972
  operator new(unsigned long) (/usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
  std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*)::{lambda(WebCore::PolicyAction)#1}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*)::{lambda(WebCore::PolicyAction)#1}> const&, std::_Manager_operation) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  webkit_web_policy_decision_new (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebKit::FrameLoaderClient::dispatchDecidePolicyForNavigationAction(WebCore::NavigationAction const&, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, std::function<void (WebCore::PolicyAction)>) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::FrameLoader::loadWithDocumentLoader(WebCore::DocumentLoader*, WebCore::FrameLoadType, WTF::PassRefPtr<WebCore::FormState>) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::FrameLoader::load(WebCore::DocumentLoader*) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  WebCore::FrameLoader::load(WebCore::FrameLoadRequest const&) (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  webkit_web_frame_load_uri (/home/user/WebKit/WebKitBuild/Release/.libs/libwebkitgtk-3.0.so.0.19.3)
  runTest(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
  runTestingServerLoop() (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
  main (/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree)
Suppression (error hash=#0449D3ED253FE1F9#):

{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znw*
   fun:_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation
   fun:webkit_web_policy_decision_new
   fun:_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE
   fun:_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_
   fun:_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE
   fun:_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE
   fun:_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE
   fun:webkit_web_frame_load_uri
   fun:_ZL7runTestRKSs
   fun:_ZL20runTestingServerLoopv
   fun:main
}


"""

valgrind_output_cppfilt_map = {
'_Znwm': u'operator new(unsigned long)',
'_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS1_15ResourceRequestEPNS1_14DocumentLoaderEN3WTF10PassRefPtrINS1_9FormStateEEEPFvPvS5_SB_bESC_EUlNS1_12PolicyActionEE_E10_M_managerERSt9_Any_dataRKSI_St18_Manager_operation': u'std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*)::{lambda(WebCore::PolicyAction)#1}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*)::{lambda(WebCore::PolicyAction)#1}> const&, std::_Manager_operation)',
'webkit_web_policy_decision_new': u'webkit_web_policy_decision_new',
'_ZN6WebKit17FrameLoaderClient39dispatchDecidePolicyForNavigationActionERKN7WebCore16NavigationActionERKNS1_15ResourceRequestEN3WTF10PassRefPtrINS1_9FormStateEEESt8functionIFvNS1_12PolicyActionEEE': u'WebKit::FrameLoaderClient::dispatchDecidePolicyForNavigationAction(WebCore::NavigationAction const&, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, std::function<void (WebCore::PolicyAction)>)',
'_ZN7WebCore13PolicyChecker21checkNavigationPolicyERKNS_15ResourceRequestEPNS_14DocumentLoaderEN3WTF10PassRefPtrINS_9FormStateEEEPFvPvS3_S9_bESA_': u'WebCore::PolicyChecker::checkNavigationPolicy(WebCore::ResourceRequest const&, WebCore::DocumentLoader*, WTF::PassRefPtr<WebCore::FormState>, void (*)(void*, WebCore::ResourceRequest const&, WTF::PassRefPtr<WebCore::FormState>, bool), void*)',
'_ZN7WebCore11FrameLoader22loadWithDocumentLoaderEPNS_14DocumentLoaderENS_13FrameLoadTypeEN3WTF10PassRefPtrINS_9FormStateEEE': u'WebCore::FrameLoader::loadWithDocumentLoader(WebCore::DocumentLoader*, WebCore::FrameLoadType, WTF::PassRefPtr<WebCore::FormState>)',
'_ZN7WebCore11FrameLoader4loadEPNS_14DocumentLoaderE': u'WebCore::FrameLoader::load(WebCore::DocumentLoader*)',
'_ZN7WebCore11FrameLoader4loadERKNS_16FrameLoadRequestE': u'WebCore::FrameLoader::load(WebCore::FrameLoadRequest const&)',
'webkit_web_frame_load_uri': u'webkit_web_frame_load_uri',
'_ZL7runTestRKSs': u'runTest(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)',
'_ZL20runTestingServerLoopv': u'runTestingServerLoop()',
'main': u'main',
'_ZNSt14_Function_base13_Base_managerIZN7WebCore13PolicyChecker18checkContentPolicyERKNS1_16ResourceResponseEPFvPvNS1_12PolicyActionEES6_EUlS7_E_E10_M_managerERSt9_Any_dataRKSC_St18_Manager_operation': u'std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*)::{lambda(WebCore::PolicyAction)#1}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*)::{lambda(WebCore::PolicyAction)#1}> const&, std::_Manager_operation)',
'webkit_web_policy_decision_new': u'webkit_web_policy_decision_new',
'_ZN6WebKit17FrameLoaderClient31dispatchDecidePolicyForResponseERKN7WebCore16ResourceResponseERKNS1_15ResourceRequestESt8functionIFvNS1_12PolicyActionEEE': u'WebKit::FrameLoaderClient::dispatchDecidePolicyForResponse(WebCore::ResourceResponse const&, WebCore::ResourceRequest const&, std::function<void (WebCore::PolicyAction)>)',
'_ZN7WebCore13PolicyChecker18checkContentPolicyERKNS_16ResourceResponseEPFvPvNS_12PolicyActionEES4_': u'WebCore::PolicyChecker::checkContentPolicy(WebCore::ResourceResponse const&, void (*)(void*, WebCore::PolicyAction), void*)',
'_ZN7WebCore14DocumentLoader16responseReceivedEPNS_14CachedResourceERKNS_16ResourceResponseE': u'WebCore::DocumentLoader::responseReceived(WebCore::CachedResource*, WebCore::ResourceResponse const&)',
'_ZN7WebCore17CachedRawResource16responseReceivedERKNS_16ResourceResponseE': u'WebCore::CachedRawResource::responseReceived(WebCore::ResourceResponse const&)',
'_ZN7WebCore17SubresourceLoader18didReceiveResponseERKNS_16ResourceResponseE': u'WebCore::SubresourceLoader::didReceiveResponse(WebCore::ResourceResponse const&)',
'_ZN7WebCoreL19sendRequestCallbackEP8_GObjectP13_GAsyncResultPv': u'WebCore::sendRequestCallback(_GObject*, _GAsyncResult*, void*)',
'g_task_return_now': u'g_task_return_now',
'complete_in_idle_cb': u'complete_in_idle_cb',
'g_main_context_dispatch': u'g_main_context_dispatch',
'g_main_context_iterate.isra.23': u'g_main_context_iterate.isra.23',
'g_main_loop_run': u'g_main_loop_run',
'gtk_main': u'gtk_main',
}


def make_mock_valgrind_results_incomplete():
    return """could not parse <?xml version="1.0"?>

<valgrindoutput>

<protocolversion>4</protocolversion>
<protocoltool>memcheck</protocoltool>

<preamble>
  <line>Memcheck, a memory error detector</line>
  <line>Copyright (C) 2002-2011, and GNU GPL'd, by Julian Seward et al.</line>
  <line>Using Valgrind-3.7.0 and LibVEX; rerun with -h for copyright info</line>
  <line>Command: /home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree -</line>
</preamble>

<pid>28531</pid>
<ppid>18577</ppid>
<tool>memcheck</tool>

<args>
  <vargv>
    <exe>/usr/bin/valgrind.bin</exe>
    <arg>--tool=memcheck</arg>
    <arg>--num-callers=40</arg>
    <arg>--demangle=no</arg>
    <arg>--trace-children=no</arg>
    <arg>--smc-check=all-non-file</arg>
    <arg>--leak-check=yes</arg>
    <arg>--leak-resolution=high</arg>
    <arg>--show-possibly-lost=no</arg>
    <arg>--show-reachable=no</arg>
    <arg>--leak-check=full</arg>
    <arg>--undef-value-errors=no</arg>
    <arg>--gen-suppressions=all</arg>
    <arg>--xml=yes</arg>
    <arg>--xml-file=/home/user/WebKit/WebKitBuild/Release/layout-test-results/drt-28531-e8c7d7b83be411e390c9d43d7e01ba08-leaks.xml</arg>
    <arg>--suppressions=/home/user/WebKit/Tools/Scripts/valgrind/suppressions.txt</arg>
    <arg>--suppressions=/usr/lib/valgrind/debian-libc6-dbg.supp</arg>
  </vargv>
  <argv>
    <exe>/home/user/WebKit/WebKitBuild/Release/Programs/DumpRenderTree</exe>
    <arg>-</arg>
  </argv>
</args>

<status>
  <state>RUNNING</state>
  <time>00:00:00:00.024 </time>
</status>
: no element found: line 49, column 0
-----------------------------------------------------
Suppressions used:
  count name
-----------------------------------------------------
"""


def make_mock_valgrind_results_empty():
    return """could not parse : no element found: line 1, column 0
-----------------------------------------------------
Suppressions used:
  count name
-----------------------------------------------------
"""


def make_mock_valgrind_results_misformatted():
    return """could not parse Junk that should not appear in a valgrind xml file<?xml version="1.0"?: syntax error: line 1, column 0
-----------------------------------------------------
Suppressions used:
  count name
-----------------------------------------------------
"""


def mock_run_cppfilt_command(args):
    if args[0] == 'c++filt':
        return valgrind_output_cppfilt_map[string_utils.decode(args[2], target_type=str)]
    return ""


class LeakDetectorValgrindTest(unittest.TestCase):

    def test_parse_and_print_leaks_detail_pass(self):
        mock_valgrind_output1 = make_mock_valgrind_output('DumpRenderTree', 28529, 'db92e4843be411e3bae1d43d7e01ba08')
        mock_valgrind_output2 = make_mock_valgrind_output('DumpRenderTree', 28530, 'dd7213423be411e3aa7fd43d7e01ba08')
        files = {}
        files['/tmp/layout-test-results/drt-28529-db92e4843be411e3bae1d43d7e01ba08-leaks.xml'] = mock_valgrind_output1
        files['/tmp/layout-test-results/drt-28530-dd7213423be411e3aa7fd43d7e01ba08-leaks.xml'] = mock_valgrind_output2

        leakdetector_valgrind = LeakDetectorValgrind(MockExecutive2(run_command_fn=mock_run_cppfilt_command), MockFileSystem(files), '/tmp/layout-test-results/')

        with OutputCapture(level=logging.INFO) as captured:
            leakdetector_valgrind.parse_and_print_leaks_detail(files)
        self.assertEqual(captured.root.log.getvalue(), make_mock_valgrind_results())

    def test_parse_and_print_leaks_detail_incomplete(self):
        mock_incomplete_valgrind_output = make_mock_incomplete_valgrind_output('DumpRenderTree', 28531, 'e8c7d7b83be411e390c9d43d7e01ba08')
        files = {}
        files['/tmp/layout-test-results/drt-28531-e8c7d7b83be411e390c9d43d7e01ba08-leaks.xml'] = mock_incomplete_valgrind_output
        leakdetector_valgrind = LeakDetectorValgrind(MockExecutive2(), MockFileSystem(files), '/tmp/layout-test-results/')

        with OutputCapture(level=logging.INFO) as captured:
            leakdetector_valgrind.parse_and_print_leaks_detail(files)
        self.assertEqual(captured.root.log.getvalue(), make_mock_valgrind_results_incomplete())

    def test_parse_and_print_leaks_detail_empty(self):
        files = {}
        files['/tmp/Logs/layout-test-results/drt-28532-ebc9a6c63be411e399d4d43d7e01ba08-leaks.xml'] = ""
        leakdetector_valgrind = LeakDetectorValgrind(MockExecutive2(), MockFileSystem(files), '/tmp/layout-test-results/')

        with OutputCapture(level=logging.INFO) as captured:
            leakdetector_valgrind.parse_and_print_leaks_detail(files)
        self.assertEqual(captured.root.log.getvalue(), make_mock_valgrind_results_empty())

    def test_parse_and_print_leaks_detail_misformatted(self):
        self.maxDiff = None
        misformatted_mock_valgrind_output = 'Junk that should not appear in a valgrind xml file' + make_mock_valgrind_output('DumpRenderTree', 28533, 'fa6d0cd63be411e39c72d43d7e01ba08')[:20]
        files = {}
        files['/tmp/layout-test-results/drt-28533-fa6d0cd63be411e39c72d43d7e01ba08-leaks.xml'] = misformatted_mock_valgrind_output
        leakdetector_valgrind = LeakDetectorValgrind(MockExecutive2(), MockFileSystem(files), '/tmp/layout-test-results/')

        with OutputCapture(level=logging.INFO) as captured:
            leakdetector_valgrind.parse_and_print_leaks_detail(files)
        self.assertEqual(captured.root.log.getvalue(), make_mock_valgrind_results_misformatted())
