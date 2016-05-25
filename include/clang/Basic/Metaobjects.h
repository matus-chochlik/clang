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
    MOC_EnumMember      = (1 <<  7) | MOC_ScopeMember,
    MOC_RecordMember    = (1 <<  8) | MOC_ScopeMember,
    MOC_Alias           = (1 <<  9) | MOC_Named|MOC_ScopeMember,
    MOC_Constant        = (1 << 10) | MOC_Typed,
    MOC_Variable        = (1 << 11) | MOC_Named|MOC_Typed|MOC_ScopeMember,
    MOC_Namespace       = (1 << 12) | MOC_Named|MOC_Scope|MOC_ScopeMember,
    MOC_GlobalScope     = (1 << 13) | MOC_Namespace,
    MOC_Type            = (1 << 14) | MOC_Named|MOC_Reversible,
    MOC_TagType         = (1 << 15) | MOC_Type|MOC_Scope|MOC_ScopeMember,
    MOC_Enum            = (1 << 16) | MOC_TagType,
    MOC_Record          = (1 << 17) | MOC_TagType,
    MOC_Class           = (1 << 18) | MOC_Record,
    MOC_Specifier       = (1 << 19) | MOC_Named,
    MOC_NamespaceAlias  = MOC_Namespace | MOC_Alias,
    MOC_TypeAlias       = MOC_Type | MOC_Alias,
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

  // When updating this also update MetaobjectOpExprBitfields
  /// \brief Names for metaobject operations
  enum MetaobjectOp {
    MOO_GetIdValue = 0,
    MOO_IsMetaObjectSequence,
    MOO_IsMetaReversible,
    MOO_IsMetaNamed,
    MOO_IsMetaTyped,
    MOO_IsMetaScope,
    MOO_IsMetaScopeMember,
    MOO_IsMetaInheritance,
    MOO_IsMetaEnumMember,
    MOO_IsMetaRecordMember,
    MOO_IsMetaAlias,
    MOO_IsMetaConstant,
    MOO_IsMetaVariable,
    MOO_IsMetaNamespace,
    MOO_IsMetaGlobalScope,
    MOO_IsMetaType,
    MOO_IsMetaTagType,
    MOO_IsMetaEnum,
    MOO_IsMetaRecord,
    MOO_IsMetaClass,
    MOO_IsMetaSpecifier,
    MOO_ReflectsSame,
    MOO_GetSourceFile,
    MOO_GetSourceLine,
    MOO_GetSourceColumn,
    MOO_IsAnonymous,
    MOO_GetName,
    MOO_GetDisplayName,
    MOO_IsScoped,
    MOO_GetScope,
    MOO_GetType,
    MOO_GetTagSpecifier,
    MOO_IsEnum,
    MOO_IsClass,
    MOO_IsStruct,
    MOO_IsUnion,
    MOO_GetBaseClasses,
    MOO_GetMemberTypes,
    MOO_GetMemberVariables,
    MOO_GetMemberConstants,
    MOO_GetBaseClass,
    MOO_GetAccessSpecifier,
    MOO_IsPublic,
    MOO_IsProtected,
    MOO_IsPrivate,
    MOO_IsStatic,
    MOO_IsVirtual,
    MOO_ExposeProtected,
    MOO_ExposePrivate,
    MOO_GetSize,
    MOO_GetElement
  };

  // When updating this also update MetaobjectOpExprBitfields
  /// \brief Names for metaobject operation results
  enum MetaobjectOpResult {
    MOOR_ULong = 0,
    MOOR_UInt,
    MOOR_Bool,
    MOOR_Enum,
    MOOR_String,
    MOOR_Metaobj
  };
}

#endif
