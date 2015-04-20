/*
  Qore Programming Language

  Copyright (C) 2003 - 2015 David Nichols

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#include "parser.cpp"
#include "scanner.cpp"
#undef ECHO
#include "Operator.cpp"
#include "QoreTreeNode.cpp"
#include "BarewordNode.cpp"
#include "SelfVarrefNode.cpp"
#include "StaticClassVarRefNode.cpp"
#include "ReferenceNode.cpp"
#include "BackquoteNode.cpp"
#include "ContextrefNode.cpp"
#include "ComplexContextrefNode.cpp"
#include "ContextRowNode.cpp"
#include "ClassRefNode.cpp"
#include "ScopedRefNode.cpp"
#include "AbstractQoreNode.cpp"
#include "QoreStringNode.cpp"
#include "DateTimeNode.cpp"
#include "qore_date_private.cpp"
#include "QoreHashNode.cpp"
#include "BinaryNode.cpp"
#include "QoreBigIntNode.cpp"
#include "QoreBoolNode.cpp"
#include "QoreFloatNode.cpp"
#include "QoreNumberNode.cpp"
#include "QoreNullNode.cpp"
#include "QoreNothingNode.cpp"
#include "VarRefNode.cpp"
#include "FunctionCallNode.cpp"
#include "QoreClosureParseNode.cpp"
#include "QoreClosureNode.cpp"
#include "QoreImplicitArgumentNode.cpp"
#include "QoreImplicitElementNode.cpp"
#include "Function.cpp"
#include "BuiltinFunctionList.cpp"
#include "GlobalVariableList.cpp"
#include "FunctionList.cpp"
#include "AbstractStatement.cpp"
#include "OnBlockExitStatement.cpp"
#include "ExpressionStatement.cpp"
#include "ReturnStatement.cpp"
#include "StatementBlock.cpp"
#include "ContextStatement.cpp"
#include "SummarizeStatement.cpp"
#include "IfStatement.cpp"
#include "WhileStatement.cpp"
#include "DoWhileStatement.cpp"
#include "ForStatement.cpp"
#include "ForEachStatement.cpp"
#include "TryStatement.cpp"
#include "ThrowStatement.cpp"
#include "SwitchStatement.cpp"
#include "Variable.cpp"
#include "support.cpp"
#ifdef HAVE_SIGNAL_HANDLING
#include "QoreSignal.cpp"
#endif
#include "QoreType.cpp"
#include "ModuleManager.cpp"
#include "QoreException.cpp"
#include "ExceptionSink.cpp"
#include "QoreClass.cpp"
#include "Context.cpp"
#include "FindNode.cpp"
#include "charset.cpp"
#include "unicode-charmaps.cpp"
#include "QoreProgram.cpp"
#include "QoreNamespace.cpp"
#include "QoreNet.cpp"
#include "QoreURL.cpp"
#include "QoreFile.cpp"
#include "QoreDir.cpp"
#include "QoreSocket.cpp"
#include "DateTime.cpp"
#include "QoreLib.cpp"
#include "QoreTimeZoneManager.cpp"
#include "QoreString.cpp"
#include "QoreObject.cpp"
#include "QoreListNode.cpp"
#include "qore-main.cpp"
#include "QoreGetOpt.cpp"
#include "QoreFtpClient.cpp"
#include "DBI.cpp"
#include "ConstantList.cpp"
#include "QoreClassList.cpp"
#include "thread.cpp"
#include "ThreadResourceList.cpp"
#include "VRMutex.cpp"
#include "VLock.cpp"
#include "QoreRWLock.cpp"
#include "AbstractSmartLock.cpp"
#include "SmartMutex.cpp"
#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
#include "CallStack.cpp"
#endif
#include "Datasource.cpp"
#include "DatasourcePool.cpp"
#include "ManagedDatasource.cpp"
#include "SQLStatement.cpp"
#include "QoreSQLStatement.cpp"
#include "ExecArgList.cpp"
#include "CallReferenceNode.cpp"
#include "NamedScope.cpp"
#include "RWLock.cpp"
#include "QoreSSLBase.cpp"
#include "QoreSSLCertificate.cpp"
#include "QoreSSLPrivateKey.cpp"
#include "QoreSocketObject.cpp"
#include "QoreCondition.cpp"
#include "QoreQueue.cpp"
#include "QoreQueueHelper.cpp"
#include "QoreRegexNode.cpp"
#include "QoreRegexBase.cpp"
#include "RegexSubstNode.cpp"
#include "RegexTransNode.cpp"
#include "Sequence.cpp"
#include "QoreReferenceCounter.cpp"
#include "QoreHTTPClient.cpp"
#include "QoreHttpClientObject.cpp"
#include "ParseOptionMap.cpp"
#include "SystemEnvironment.cpp"
#include "QoreCounter.cpp"
#include "ReferenceArgumentHelper.cpp"
#include "ReferenceHelper.cpp"
#include "QoreTypeInfo.cpp"
#include "QoreDeleteOperatorNode.cpp"
#include "QoreRemoveOperatorNode.cpp"
#include "QoreSpliceOperatorNode.cpp"
#include "QoreExtractOperatorNode.cpp"
#include "QoreCastOperatorNode.cpp"
#include "QoreUnaryMinusOperatorNode.cpp"
#include "QoreDotEvalOperatorNode.cpp"
#include "QoreLogicalEqualsOperatorNode.cpp"
#include "QoreAssignmentOperatorNode.cpp"
#include "QoreIntAssignmentOperatorNode.cpp"
#include "QorePlusEqualsOperatorNode.cpp"
#include "QoreIntPlusEqualsOperatorNode.cpp"
#include "QoreMinusEqualsOperatorNode.cpp"
#include "QoreIntMinusEqualsOperatorNode.cpp"
#include "QoreOrEqualsOperatorNode.cpp"
#include "QoreAndEqualsOperatorNode.cpp"
#include "QoreModulaEqualsOperatorNode.cpp"
#include "QoreMultiplyEqualsOperatorNode.cpp"
#include "QoreDivideEqualsOperatorNode.cpp"
#include "QoreXorEqualsOperatorNode.cpp"
#include "QoreShiftLeftEqualsOperatorNode.cpp"
#include "QoreShiftRightEqualsOperatorNode.cpp"
#include "QorePostIncrementOperatorNode.cpp"
#include "QoreIntPostIncrementOperatorNode.cpp"
#include "QorePostDecrementOperatorNode.cpp"
#include "QoreIntPostDecrementOperatorNode.cpp"
#include "QorePreIncrementOperatorNode.cpp"
#include "QoreIntPreIncrementOperatorNode.cpp"
#include "QorePreDecrementOperatorNode.cpp"
#include "QoreIntPreDecrementOperatorNode.cpp"
#include "QoreLogicalLessThanOperatorNode.cpp"
#include "QoreLogicalGreaterThanOperatorNode.cpp"
#include "QoreMapOperatorNode.cpp"
#include "QoreMapSelectOperatorNode.cpp"
#include "QoreHashMapOperatorNode.cpp"
#include "QoreHashMapSelectOperatorNode.cpp"
#include "QoreNullCoalescingOperatorNode.cpp"
#include "QoreValueCoalescingOperatorNode.cpp"
#include "ql_thread.cpp"
#include "ql_time.cpp"
#include "ql_lib.cpp"
#include "ql_math.cpp"
#include "ql_type.cpp"
#include "ql_env.cpp"
#include "ql_string.cpp"
#include "ql_pwd.cpp"
#include "ql_misc.cpp"
#include "ql_list.cpp"
#include "ql_crypto.cpp"
#include "ql_object.cpp"
#include "ql_file.cpp"
#include "ql_compression.cpp"
#include "ql_dbi.cpp"
#include "ql_context.cpp"
#include "qc_option.cpp"
#include "qc_errno.cpp"
#include "qc_qore.cpp"
#include "QC_Socket.cpp"
#include "QC_Program.cpp"
#include "QC_ReadOnlyFile.cpp"
#include "QC_File.cpp"
#include "QC_Dir.cpp"
#include "QC_GetOpt.cpp"
#include "QC_FtpClient.cpp"
#include "QC_AbstractIterator.cpp"
#include "QC_AbstractQuantifiedIterator.cpp"
#include "QC_AbstractBidirectionalIterator.cpp"
#include "QC_AbstractQuantifiedBidirectionalIterator.cpp"
#include "QC_ListIterator.cpp"
#include "QC_ListReverseIterator.cpp"
#include "QC_HashIterator.cpp"
#include "QC_HashReverseIterator.cpp"
#include "QC_HashKeyIterator.cpp"
#include "QC_HashKeyReverseIterator.cpp"
#include "QC_HashPairIterator.cpp"
#include "QC_HashPairReverseIterator.cpp"
#include "QC_HashListIterator.cpp"
#include "QC_HashListReverseIterator.cpp"
#include "QC_ListHashIterator.cpp"
#include "QC_ListHashReverseIterator.cpp"
#include "QC_ObjectIterator.cpp"
#include "QC_ObjectReverseIterator.cpp"
#include "QC_ObjectKeyIterator.cpp"
#include "QC_ObjectKeyReverseIterator.cpp"
#include "QC_ObjectPairIterator.cpp"
#include "QC_ObjectPairReverseIterator.cpp"
#include "QC_FileLineIterator.cpp"
#include "QC_DataLineIterator.cpp"
#include "QC_SingleValueIterator.cpp"
#include "QC_RangeIterator.cpp"
#include "QC_ThreadPool.cpp"
#include "QC_AbstractDatasource.cpp"
#include "QC_Datasource.cpp"
#include "QC_DatasourcePool.cpp"
#include "QC_SQLStatement.cpp"
#include "QC_Queue.cpp"
#include "QC_Mutex.cpp"
#include "QC_Condition.cpp"
#include "QC_RWLock.cpp"
#include "QC_Gate.cpp"
#include "QC_Sequence.cpp"
#include "QC_Counter.cpp"
#include "QC_SSLCertificate.cpp"
#include "QC_SSLPrivateKey.cpp"
#include "QC_HTTPClient.cpp"
#include "QC_AutoLock.cpp"
#include "QC_AutoGate.cpp"
#include "QC_AutoReadLock.cpp"
#include "QC_AutoWriteLock.cpp"
#include "QC_TermIOS.cpp"
#include "QC_AbstractSmartLock.cpp"
#include "QC_TimeZone.cpp"

#include "QorePseudoMethods.cpp"

#include "xxhash.cpp"

#ifndef HAVE_GLOB
#include "glob.cpp"
#endif
#ifndef HAVE_INET_NTOP
#include "inet_ntop.cpp"
#endif
#ifndef HAVE_INET_PTON
#include "inet_pton.cpp"
#endif
#include "minitest.cpp"
//#include "inline_printf.cpp"
//#include "QException.h"

#ifdef DEBUG
#include "ql_debug.cpp"
#endif

