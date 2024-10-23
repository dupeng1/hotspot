/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#include "precompiled.hpp"
#include "classfile/symbolTable.hpp"
#include "code/icBuffer.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/bytecodes.hpp"
#include "memory/universe.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/init.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/sharedRuntime.hpp"
#include "utilities/macros.hpp"

// Initialization done by VM thread in vm_init_globals()
void check_ThreadShadow();
void eventlog_init();
void mutex_init();
void chunkpool_init();
void perfMemory_init();

// Initialization done by Java thread in init_globals()
void management_init();
void bytecodes_init();
void classLoader_init();
void codeCache_init();
void VM_Version_init();
void os_init_globals();        // depends on VM_Version_init, before universe_init
void stubRoutines_init1();
jint universe_init();          // depends on codeCache_init and stubRoutines_init
void interpreter_init();       // before any methods loaded
void invocationCounter_init(); // before any methods loaded
void marksweep_init();
void accessFlags_init();
void templateTable_init();
void InterfaceSupport_init();
void universe2_init();  // dependent on codeCache_init and stubRoutines_init, loads primordial classes
void referenceProcessor_init();
void jni_handles_init();
void vmStructs_init();

void vtableStubs_init();
void InlineCacheBuffer_init();
void compilerOracle_init();
void compilationPolicy_init();
void compileBroker_init();

// Initialization after compiler initialization
bool universe_post_init();  // must happen after compiler_init
void javaClasses_init();  // must happen after vtable initialization
void stubRoutines_init2(); // note: StubRoutines need 2-phase init

// Do not disable thread-local-storage, as it is important for some
// JNI/JVM/JVMTI functions and signal handlers to work properly
// during VM shutdown
void perfMemory_exit();
void ostream_exit();

void vm_init_globals() {
  // threadshadow初始化,该类是处理线程的exception,是所有线程类的父类.线程的exception是从外部挂上去的,
  // 这个挂载就是threadshadow类,所以就像是影子一样如影随从.
  check_ThreadShadow();
  // basic_types初始化,基本类型的初始化,判断类型和大小是否是正确
  basic_types_init();
  // 事件日志,eventlog的初始化,全部事件有消息,违例,多重定义,类未加载和破环优化的消息.
  eventlog_init();
  // 互斥锁初始化,使用宏定义来预先定义了所有的互斥锁类型
  mutex_init();
  // 内存分配器,小块内存初始化
  chunkpool_init();
  // 性能统计数据内存,永久区内存初始化
  perfMemory_init();
}

// init_globals是最核心的初始化方法,几乎包括了所有的jvm核心系统
jint init_globals() {
  HandleMark hm;
    // 管理模块初始化，包括时间统计、各种指标计数、性能数据统计、运行时数据统计和监控、类加载服务情况（加载类数量、加载类失败数量、加载字节数等）
  management_init();
    // 字节码初始化。Hotspot执行字节码时，大部分是解释执行，这就需要将字节码转换成机器码来执行，执行前需要知道字节码是什么格式、类型、占用长度、结果类型等信息，这一步就是做这些初始化工作的
  bytecodes_init();
    // 类加载器初始化
  classLoader_init();
    // 代码缓存空间初始化。虚拟机为了提升执行绩效，会把一些热点指令代码提前编译为机器码并缓存起来，这一步就是做这个操作的
  codeCache_init();
    // 虚拟机版本号初始化
  VM_Version_init();
    // os的扩展初始化，jdk8版本的hotspot里，这个函数没做任何具体实现
  os_init_globals(); //调用interpreter_init()时，已触发TemplateTable::initialize()
    // JVM虚拟机调用Java程序时需要借助stub存根来处理，大家做rpc/webservice就了解stub存根的意义，就是对远程调用函数的一个声明/映射，这里的stubRoutines的意思与这个差不多，这个比较关键，后面章节会重点细讲
  stubRoutines_init1();
    // universe 翻译就是万物的意思，这一步初始化，主要是对元空间内存、各种符号表、字符表、Java堆内存空间
  jint status = universe_init();  // dependent on codeCache_init and
                                  // stubRoutines_init1 and metaspace_init.
  if (status != JNI_OK)
    return status;
  // 解释器初始化
  interpreter_init();  // before any methods loaded
    // 方法调用计数初始化
  invocationCounter_init();  // before any methods loaded
    // 标记清除初始化，GC相关的，后面讲GC时再讲
  marksweep_init();
  // 这个仅仅是校验AccessFlags的大小，里面是一个assert断言
  accessFlags_init();
    // 模板表 TemplateTable 的初始化，TemplateTable中保存了各个字节码的执行模板（目标代码生成函数和参数）
  templateTable_init(); 
  // 这个函数没做什么，就是通过系统调用srand设置了随机数的随机种子，方便rand()调用时根据种子生成一个伪随机数
  InterfaceSupport_init();
    // 生成运行时的一些stub函数，例如错误方法处理的stub、静态解析处理的stub等
  SharedRuntime::generate_stubs();
    // universe_init的进一步初始化，里面内容很多，主要概括为对基础数据类型、数组类型、对象类型创建对应的Klass对象、bootstrap类加载器的初始化等
  universe2_init();  // dependent on codeCache_init and stubRoutines_init1
    // 引用处理器初始化
  referenceProcessor_init();
    // JNI句柄分配空间块并初始化
  jni_handles_init();
#if INCLUDE_VM_STRUCTS
  vmStructs_init();// vm内部数据结构的初始化
#endif // INCLUDE_VM_STRUCTS
  // 初始化vtable数组的大小，并预分配数组空间，vtable（虚函数表）是c++中对虚函数表的存储和表示
  vtableStubs_init();
    // 代码缓冲区初始化
  InlineCacheBuffer_init();
    // oracle编译器初始化
  compilerOracle_init();
    // 代理编译初始化，主要做两件事：1.选择编译器；2.如何进行编译
  compilationPolicy_init();
    // 编译日志记录
  compileBroker_init();
    // 初始化寄存器数组，给每个寄存器初始化名字，汇编代码需要
  VMRegImpl::set_regName();
  // 初始化universe后的逻辑操作，包括加载基础类、构建报错信息、安全检查、加载器、引用管理等，用过spring的就知道里面有很多post后置逻辑操作
  if (!universe_post_init()) {
    return JNI_ERR;
  }
  // 系统及java class的初始化
  javaClasses_init();   // must happen after vtable initialization
    // stubRoutines的第二阶段初始化
  stubRoutines_init2(); // note: StubRoutines need 2-phase init

  // All the flags that get adjusted by VM_Version_init and os::init_2
  // have been set so dump the flags now.
  //��Ӧ-XX:+PrintFlagsFinal����-XX:+PrintFlagsWithComments���ƣ�
  //���ߵ���printFlagsʱ�ڶ���������true��������share\vm\runtime\arguments.cpp��parse�о͵����ˣ�Ȼ��vm�����˳�
  if (PrintFlagsFinal) {
    CommandLineFlags::printFlags(tty, false);
  }

  return JNI_OK;
}


void exit_globals() {
  static bool destructorsCalled = false;
  if (!destructorsCalled) {
    destructorsCalled = true;
    perfMemory_exit();
    if (PrintSafepointStatistics) {
      // Print the collected safepoint statistics.
      SafepointSynchronize::print_stat_on_exit();
    }
    if (PrintStringTableStatistics) {
      SymbolTable::dump(tty);
      StringTable::dump(tty);
    }
    ostream_exit();
  }
}


static bool _init_completed = false;

bool is_init_completed() {
  return _init_completed;
}


void set_init_completed() {
  assert(Universe::is_fully_initialized(), "Should have completed initialization");
  _init_completed = true;
}
