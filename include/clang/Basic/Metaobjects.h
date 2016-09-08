//===---- Metaobjects.h - C++ Reflection Support Enumerations ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines enumerations for the metaobject support.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_METAOBJECTS_H
#define LLVM_CLANG_BASIC_METAOBJECTS_H

namespace clang {

  // When updating this also update METAOBJECT_TRAIT in TokenKinds.def
  enum MetaobjectConcept  {
    MOC_ObjectSequence  = (1 <<  0),
    MOC_Reversible      = (1 <<  1),
    MOC_Named           = (1 <<  2),
    MOC_Typed           = (1 <<  3),
    MOC_Scope           = (1 <<  4),
    MOC_ScopeMember     = (1 <<  5),
    MOC_Inheritance     = (1 <<  6),
    MOC_Template        = (1 <<  7),
    MOC_Parameter       = (1 <<  8),
    MOC_EnumMember      = (1 <<  9) | MOC_ScopeMember,
    MOC_RecordMember    = (1 << 10) | MOC_ScopeMember,
    MOC_Alias           = (1 << 11) | MOC_Named,
    MOC_Constant        = (1 << 12) | MOC_Typed,
    MOC_Variable        = (1 << 13) | MOC_Named | MOC_Typed | MOC_ScopeMember,
    MOC_Namespace       = (1 << 14) | MOC_Named | MOC_Scope | MOC_ScopeMember,
    MOC_GlobalScope     = (1 << 15) | MOC_Namespace,
    MOC_Type            = (1 << 16) | MOC_Named | MOC_Reversible,
    MOC_TagType         = (1 << 17) | MOC_Type | MOC_Scope | MOC_ScopeMember,
    MOC_Enum            = (1 << 18) | MOC_TagType,
    MOC_Record          = (1 << 19) | MOC_TagType,
    MOC_Class           = (1 << 20) | MOC_Record,
    MOC_Specifier       = (1 << 21) | MOC_Named,
    MOC_TplTypeParam    = MOC_Template | MOC_Type| MOC_Parameter | MOC_Alias,
    MOC_NamespaceAlias  = MOC_Namespace | MOC_Alias,
    MOC_TypeAlias       = MOC_Type | MOC_Alias | MOC_ScopeMember,
    MOC_EnumAlias       = MOC_Enum | MOC_Alias,
    MOC_ClassAlias      = MOC_Class | MOC_Alias,
    MOC_DataMember      = MOC_RecordMember | MOC_Variable,
    MOC_MemberType      = MOC_RecordMember | MOC_Type,
    MOC_MemberClass     = MOC_RecordMember | MOC_Class,
    MOC_MemberEnum      = MOC_RecordMember | MOC_Enum,
    MOC_Enumerator      = MOC_Constant | MOC_Named | MOC_EnumMember
  };

  // When updating this also update ReflexprExprBitfields
  enum MetaobjectKind {
    MOK_Unknown = 0,
    MOK_ObjectSequence,
    MOK_Inheritance,
    MOK_Specifier,
    MOK_GlobalScope,
    MOK_Namespace,
    MOK_Type,
    MOK_Enum,
    MOK_Class,
    MOK_NamespaceAlias,
    MOK_TypeAlias,
    MOK_EnumAlias,
    MOK_ClassAlias,
    MOK_TplTypeParam,
    MOK_Variable,
    MOK_DataMember,
    MOK_MemberType,
    MOK_Enumerator
  };

  enum MetaobjectSequenceKind {
    MOSK_None = 0,
    MOSK_MemberConstants,
    MOSK_MemberVariables,
    MOSK_MemberTypes,
    MOSK_BaseClasses,
    MOSK_All
  };

  // When updating this also update UnaryMetaobjectOpExprBitfields
  /// \brief Names for unary metaobject operations
  enum UnaryMetaobjectOp {
    UMOO_GetIdValue = 0,
    UMOO_IsMetaObjectSequence,
    UMOO_IsMetaReversible,
    UMOO_IsMetaNamed,
    UMOO_IsMetaTyped,
    UMOO_IsMetaScope,
    UMOO_IsMetaScopeMember,
    UMOO_IsMetaInheritance,
    UMOO_IsMetaTemplate,
    UMOO_IsMetaParameter,
    UMOO_IsMetaEnumMember,
    UMOO_IsMetaRecordMember,
    UMOO_IsMetaAlias,
    UMOO_IsMetaConstant,
    UMOO_IsMetaVariable,
    UMOO_IsMetaNamespace,
    UMOO_IsMetaGlobalScope,
    UMOO_IsMetaType,
    UMOO_IsMetaTagType,
    UMOO_IsMetaEnum,
    UMOO_IsMetaRecord,
    UMOO_IsMetaClass,
    UMOO_IsMetaSpecifier,
    UMOO_GetSourceFile,
    UMOO_GetSourceLine,
    UMOO_GetSourceColumn,
    UMOO_GetUnderlyingObject,
    UMOO_IsAnonymous,
    UMOO_GetBaseName,
    UMOO_GetDisplayName,
    UMOO_IsScopedEnum,
    UMOO_GetScope,
    UMOO_GetType,
    UMOO_GetAliased,
    UMOO_GetTagSpecifier,
    UMOO_IsEnum,
    UMOO_IsClass,
    UMOO_IsStruct,
    UMOO_IsUnion,
    UMOO_GetBaseClasses,
    UMOO_GetMemberTypes,
    UMOO_GetMemberVariables,
    UMOO_GetMemberConstants,
    UMOO_GetBaseClass,
    UMOO_GetAccessSpecifier,
    UMOO_IsPublic,
    UMOO_IsProtected,
    UMOO_IsPrivate,
    UMOO_IsStatic,
    UMOO_IsVirtual,
    UMOO_GetPointer,
    UMOO_GetConstant,
    UMOO_HideProtected,
    UMOO_HidePrivate,
    UMOO_GetSize
  };

  // When updating this also update NaryMetaobjectOpExprBitfields
  /// \brief Names for n-ary metaobject operations
  enum NaryMetaobjectOp {
    NMOO_ReflectsSame,
    NMOO_GetElement
  };

  // When updating this also update MetaobjectOpExprBitfields
  /// \brief Names for metaobject operation results
  enum MetaobjectOpResult {
    MOOR_ULong = 0,
    MOOR_UInt,
    MOOR_Bool,
    MOOR_Const,
    MOOR_String,
    MOOR_Pointer,
    MOOR_Metaobj
  };
}

#endif
