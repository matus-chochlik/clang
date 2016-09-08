//===- ExprCXX.cpp - (C++) Expression AST Node Implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the subclesses of Expr class declared in ExprCXX.h
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprCXX.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclAccessPair.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DeclarationName.h"
#include "clang/AST/Expr.h"
#include "clang/AST/LambdaCapture.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/TemplateBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/OperatorKinds.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>

using namespace clang;

//===----------------------------------------------------------------------===//
//  Child Iterators for iterating over subexpressions/substatements
//===----------------------------------------------------------------------===//

bool CXXOperatorCallExpr::isInfixBinaryOp() const {
  // An infix binary operator is any operator with two arguments other than
  // operator() and operator[]. Note that none of these operators can have
  // default arguments, so it suffices to check the number of argument
  // expressions.
  if (getNumArgs() != 2)
    return false;

  switch (getOperator()) {
  case OO_Call:
  case OO_Subscript:
    return false;
  default:
    return true;
  }
}

bool CXXTypeidExpr::isPotentiallyEvaluated() const {
  if (isTypeOperand())
    return false;

  // C++11 [expr.typeid]p3:
  //   When typeid is applied to an expression other than a glvalue of
  //   polymorphic class type, [...] the expression is an unevaluated operand.
  const Expr *E = getExprOperand();
  if (const CXXRecordDecl *RD = E->getType()->getAsCXXRecordDecl())
    if (RD->isPolymorphic() && E->isGLValue())
      return true;

  return false;
}

QualType CXXTypeidExpr::getTypeOperand(ASTContext &Context) const {
  assert(isTypeOperand() && "Cannot call getTypeOperand for typeid(expr)");
  Qualifiers Quals;
  return Context.getUnqualifiedArrayType(
      Operand.get<TypeSourceInfo *>()->getType().getNonReferenceType(), Quals);
}

QualType CXXUuidofExpr::getTypeOperand(ASTContext &Context) const {
  assert(isTypeOperand() && "Cannot call getTypeOperand for __uuidof(expr)");
  Qualifiers Quals;
  return Context.getUnqualifiedArrayType(
      Operand.get<TypeSourceInfo *>()->getType().getNonReferenceType(), Quals);
}

// CXXScalarValueInitExpr
SourceLocation CXXScalarValueInitExpr::getBeginLoc() const {
  return TypeInfo ? TypeInfo->getTypeLoc().getBeginLoc() : RParenLoc;
}

// CXXNewExpr
CXXNewExpr::CXXNewExpr(const ASTContext &C, bool globalNew,
                       FunctionDecl *operatorNew, FunctionDecl *operatorDelete,
                       bool PassAlignment, bool usualArrayDeleteWantsSize,
                       ArrayRef<Expr *> placementArgs, SourceRange typeIdParens,
                       Expr *arraySize, InitializationStyle initializationStyle,
                       Expr *initializer, QualType ty,
                       TypeSourceInfo *allocatedTypeInfo, SourceRange Range,
                       SourceRange directInitRange)
    : Expr(CXXNewExprClass, ty, VK_RValue, OK_Ordinary, ty->isDependentType(),
           ty->isDependentType(), ty->isInstantiationDependentType(),
           ty->containsUnexpandedParameterPack()),
      OperatorNew(operatorNew), OperatorDelete(operatorDelete),
      AllocatedTypeInfo(allocatedTypeInfo), TypeIdParens(typeIdParens),
      Range(Range), DirectInitRange(directInitRange), GlobalNew(globalNew),
      PassAlignment(PassAlignment),
      UsualArrayDeleteWantsSize(usualArrayDeleteWantsSize) {
  assert((initializer != nullptr || initializationStyle == NoInit) &&
         "Only NoInit can have no initializer.");
  StoredInitializationStyle = initializer ? initializationStyle + 1 : 0;
  AllocateArgsArray(C, arraySize != nullptr, placementArgs.size(),
                    initializer != nullptr);
  unsigned i = 0;
  if (Array) {
    if (arraySize->isInstantiationDependent())
      ExprBits.InstantiationDependent = true;

    if (arraySize->containsUnexpandedParameterPack())
      ExprBits.ContainsUnexpandedParameterPack = true;

    SubExprs[i++] = arraySize;
  }

  if (initializer) {
    if (initializer->isInstantiationDependent())
      ExprBits.InstantiationDependent = true;

    if (initializer->containsUnexpandedParameterPack())
      ExprBits.ContainsUnexpandedParameterPack = true;

    SubExprs[i++] = initializer;
  }

  for (unsigned j = 0; j != placementArgs.size(); ++j) {
    if (placementArgs[j]->isInstantiationDependent())
      ExprBits.InstantiationDependent = true;
    if (placementArgs[j]->containsUnexpandedParameterPack())
      ExprBits.ContainsUnexpandedParameterPack = true;

    SubExprs[i++] = placementArgs[j];
  }

  switch (getInitializationStyle()) {
  case CallInit:
    this->Range.setEnd(DirectInitRange.getEnd());
    break;
  case ListInit:
    this->Range.setEnd(getInitializer()->getSourceRange().getEnd());
    break;
  default:
    if (TypeIdParens.isValid())
      this->Range.setEnd(TypeIdParens.getEnd());
    break;
  }
}

void CXXNewExpr::AllocateArgsArray(const ASTContext &C, bool isArray,
                                   unsigned numPlaceArgs, bool hasInitializer) {
  assert(SubExprs == nullptr && "SubExprs already allocated");
  Array = isArray;
  NumPlacementArgs = numPlaceArgs;

  unsigned TotalSize = Array + hasInitializer + NumPlacementArgs;
  SubExprs = new (C) Stmt *[TotalSize];
}

bool CXXNewExpr::shouldNullCheckAllocation(const ASTContext &Ctx) const {
  return getOperatorNew()
             ->getType()
             ->castAs<FunctionProtoType>()
             ->isNothrow() &&
         !getOperatorNew()->isReservedGlobalPlacementOperator();
}

// CXXDeleteExpr
QualType CXXDeleteExpr::getDestroyedType() const {
  const Expr *Arg = getArgument();

  // For a destroying operator delete, we may have implicitly converted the
  // pointer type to the type of the parameter of the 'operator delete'
  // function.
  while (const auto *ICE = dyn_cast<ImplicitCastExpr>(Arg)) {
    if (ICE->getCastKind() == CK_DerivedToBase ||
        ICE->getCastKind() == CK_UncheckedDerivedToBase ||
        ICE->getCastKind() == CK_NoOp) {
      assert((ICE->getCastKind() == CK_NoOp ||
              getOperatorDelete()->isDestroyingOperatorDelete()) &&
             "only a destroying operator delete can have a converted arg");
      Arg = ICE->getSubExpr();
    } else
      break;
  }

  // The type-to-delete may not be a pointer if it's a dependent type.
  const QualType ArgType = Arg->getType();

  if (ArgType->isDependentType() && !ArgType->isPointerType())
    return QualType();

  return ArgType->getAs<PointerType>()->getPointeeType();
}

// CXXPseudoDestructorExpr
PseudoDestructorTypeStorage::PseudoDestructorTypeStorage(TypeSourceInfo *Info)
    : Type(Info) {
  Location = Info->getTypeLoc().getLocalSourceRange().getBegin();
}

CXXPseudoDestructorExpr::CXXPseudoDestructorExpr(
    const ASTContext &Context, Expr *Base, bool isArrow,
    SourceLocation OperatorLoc, NestedNameSpecifierLoc QualifierLoc,
    TypeSourceInfo *ScopeType, SourceLocation ColonColonLoc,
    SourceLocation TildeLoc, PseudoDestructorTypeStorage DestroyedType)
    : Expr(CXXPseudoDestructorExprClass, Context.BoundMemberTy, VK_RValue,
           OK_Ordinary,
           /*isTypeDependent=*/
           (Base->isTypeDependent() ||
            (DestroyedType.getTypeSourceInfo() &&
             DestroyedType.getTypeSourceInfo()->getType()->isDependentType())),
           /*isValueDependent=*/Base->isValueDependent(),
           (Base->isInstantiationDependent() ||
            (QualifierLoc && QualifierLoc.getNestedNameSpecifier()
                                 ->isInstantiationDependent()) ||
            (ScopeType &&
             ScopeType->getType()->isInstantiationDependentType()) ||
            (DestroyedType.getTypeSourceInfo() &&
             DestroyedType.getTypeSourceInfo()
                 ->getType()
                 ->isInstantiationDependentType())),
           // ContainsUnexpandedParameterPack
           (Base->containsUnexpandedParameterPack() ||
            (QualifierLoc && QualifierLoc.getNestedNameSpecifier()
                                 ->containsUnexpandedParameterPack()) ||
            (ScopeType &&
             ScopeType->getType()->containsUnexpandedParameterPack()) ||
            (DestroyedType.getTypeSourceInfo() &&
             DestroyedType.getTypeSourceInfo()
                 ->getType()
                 ->containsUnexpandedParameterPack()))),
      Base(static_cast<Stmt *>(Base)), IsArrow(isArrow),
      OperatorLoc(OperatorLoc), QualifierLoc(QualifierLoc),
      ScopeType(ScopeType), ColonColonLoc(ColonColonLoc), TildeLoc(TildeLoc),
      DestroyedType(DestroyedType) {}

QualType CXXPseudoDestructorExpr::getDestroyedType() const {
  if (TypeSourceInfo *TInfo = DestroyedType.getTypeSourceInfo())
    return TInfo->getType();

  return QualType();
}

SourceLocation CXXPseudoDestructorExpr::getEndLoc() const {
  SourceLocation End = DestroyedType.getLocation();
  if (TypeSourceInfo *TInfo = DestroyedType.getTypeSourceInfo())
    End = TInfo->getTypeLoc().getLocalSourceRange().getEnd();
  return End;
}

// UnresolvedLookupExpr
UnresolvedLookupExpr *UnresolvedLookupExpr::Create(
    const ASTContext &C, CXXRecordDecl *NamingClass,
    NestedNameSpecifierLoc QualifierLoc, SourceLocation TemplateKWLoc,
    const DeclarationNameInfo &NameInfo, bool ADL,
    const TemplateArgumentListInfo *Args, UnresolvedSetIterator Begin,
    UnresolvedSetIterator End) {
  assert(Args || TemplateKWLoc.isValid());
  unsigned num_args = Args ? Args->size() : 0;

  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(1,
                                                                      num_args);
  void *Mem = C.Allocate(Size, alignof(UnresolvedLookupExpr));
  return new (Mem)
      UnresolvedLookupExpr(C, NamingClass, QualifierLoc, TemplateKWLoc,
                           NameInfo, ADL, /*Overload*/ true, Args, Begin, End);
}

UnresolvedLookupExpr *
UnresolvedLookupExpr::CreateEmpty(const ASTContext &C,
                                  bool HasTemplateKWAndArgsInfo,
                                  unsigned NumTemplateArgs) {
  assert(NumTemplateArgs == 0 || HasTemplateKWAndArgsInfo);
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, NumTemplateArgs);
  void *Mem = C.Allocate(Size, alignof(UnresolvedLookupExpr));
  auto *E = new (Mem) UnresolvedLookupExpr(EmptyShell());
  E->HasTemplateKWAndArgsInfo = HasTemplateKWAndArgsInfo;
  return E;
}

OverloadExpr::OverloadExpr(
    StmtClass K, const ASTContext &C, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateKWLoc, const DeclarationNameInfo &NameInfo,
    const TemplateArgumentListInfo *TemplateArgs, UnresolvedSetIterator Begin,
    UnresolvedSetIterator End, bool KnownDependent,
    bool KnownInstantiationDependent, bool KnownContainsUnexpandedParameterPack)
    : Expr(
          K, C.OverloadTy, VK_LValue, OK_Ordinary, KnownDependent,
          KnownDependent,
          (KnownInstantiationDependent || NameInfo.isInstantiationDependent() ||
           (QualifierLoc &&
            QualifierLoc.getNestedNameSpecifier()->isInstantiationDependent())),
          (KnownContainsUnexpandedParameterPack ||
           NameInfo.containsUnexpandedParameterPack() ||
           (QualifierLoc && QualifierLoc.getNestedNameSpecifier()
                                ->containsUnexpandedParameterPack()))),
      NameInfo(NameInfo), QualifierLoc(QualifierLoc), NumResults(End - Begin),
      HasTemplateKWAndArgsInfo(TemplateArgs != nullptr ||
                               TemplateKWLoc.isValid()) {
  NumResults = End - Begin;
  if (NumResults) {
    // Determine whether this expression is type-dependent.
    for (UnresolvedSetImpl::const_iterator I = Begin; I != End; ++I) {
      if ((*I)->getDeclContext()->isDependentContext() ||
          isa<UnresolvedUsingValueDecl>(*I)) {
        ExprBits.TypeDependent = true;
        ExprBits.ValueDependent = true;
        ExprBits.InstantiationDependent = true;
      }
    }

    Results = static_cast<DeclAccessPair *>(C.Allocate(
        sizeof(DeclAccessPair) * NumResults, alignof(DeclAccessPair)));
    memcpy(Results, Begin.I, NumResults * sizeof(DeclAccessPair));
  }

  // If we have explicit template arguments, check for dependent
  // template arguments and whether they contain any unexpanded pack
  // expansions.
  if (TemplateArgs) {
    bool Dependent = false;
    bool InstantiationDependent = false;
    bool ContainsUnexpandedParameterPack = false;
    getTrailingASTTemplateKWAndArgsInfo()->initializeFrom(
        TemplateKWLoc, *TemplateArgs, getTrailingTemplateArgumentLoc(),
        Dependent, InstantiationDependent, ContainsUnexpandedParameterPack);

    if (Dependent) {
      ExprBits.TypeDependent = true;
      ExprBits.ValueDependent = true;
    }
    if (InstantiationDependent)
      ExprBits.InstantiationDependent = true;
    if (ContainsUnexpandedParameterPack)
      ExprBits.ContainsUnexpandedParameterPack = true;
  } else if (TemplateKWLoc.isValid()) {
    getTrailingASTTemplateKWAndArgsInfo()->initializeFrom(TemplateKWLoc);
  }

  if (isTypeDependent())
    setType(C.DependentTy);
}

void OverloadExpr::initializeResults(const ASTContext &C,
                                     UnresolvedSetIterator Begin,
                                     UnresolvedSetIterator End) {
  assert(!Results && "Results already initialized!");
  NumResults = End - Begin;
  if (NumResults) {
    Results = static_cast<DeclAccessPair *>(
        C.Allocate(sizeof(DeclAccessPair) * NumResults,

                   alignof(DeclAccessPair)));
    memcpy(Results, Begin.I, NumResults * sizeof(DeclAccessPair));
  }
}

CXXRecordDecl *OverloadExpr::getNamingClass() const {
  if (isa<UnresolvedLookupExpr>(this))
    return cast<UnresolvedLookupExpr>(this)->getNamingClass();
  else
    return cast<UnresolvedMemberExpr>(this)->getNamingClass();
}

// DependentScopeDeclRefExpr
DependentScopeDeclRefExpr::DependentScopeDeclRefExpr(
    QualType T, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateKWLoc, const DeclarationNameInfo &NameInfo,
    const TemplateArgumentListInfo *Args)
    : Expr(
          DependentScopeDeclRefExprClass, T, VK_LValue, OK_Ordinary, true, true,
          (NameInfo.isInstantiationDependent() ||
           (QualifierLoc &&
            QualifierLoc.getNestedNameSpecifier()->isInstantiationDependent())),
          (NameInfo.containsUnexpandedParameterPack() ||
           (QualifierLoc && QualifierLoc.getNestedNameSpecifier()
                                ->containsUnexpandedParameterPack()))),
      QualifierLoc(QualifierLoc), NameInfo(NameInfo),
      HasTemplateKWAndArgsInfo(Args != nullptr || TemplateKWLoc.isValid()) {
  if (Args) {
    bool Dependent = true;
    bool InstantiationDependent = true;
    bool ContainsUnexpandedParameterPack =
        ExprBits.ContainsUnexpandedParameterPack;
    getTrailingObjects<ASTTemplateKWAndArgsInfo>()->initializeFrom(
        TemplateKWLoc, *Args, getTrailingObjects<TemplateArgumentLoc>(),
        Dependent, InstantiationDependent, ContainsUnexpandedParameterPack);
    ExprBits.ContainsUnexpandedParameterPack = ContainsUnexpandedParameterPack;
  } else if (TemplateKWLoc.isValid()) {
    getTrailingObjects<ASTTemplateKWAndArgsInfo>()->initializeFrom(
        TemplateKWLoc);
  }
}

DependentScopeDeclRefExpr *DependentScopeDeclRefExpr::Create(
    const ASTContext &C, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateKWLoc, const DeclarationNameInfo &NameInfo,
    const TemplateArgumentListInfo *Args) {
  assert(QualifierLoc && "should be created for dependent qualifiers");
  bool HasTemplateKWAndArgsInfo = Args || TemplateKWLoc.isValid();
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, Args ? Args->size() : 0);
  void *Mem = C.Allocate(Size);
  return new (Mem) DependentScopeDeclRefExpr(C.DependentTy, QualifierLoc,
                                             TemplateKWLoc, NameInfo, Args);
}

DependentScopeDeclRefExpr *
DependentScopeDeclRefExpr::CreateEmpty(const ASTContext &C,
                                       bool HasTemplateKWAndArgsInfo,
                                       unsigned NumTemplateArgs) {
  assert(NumTemplateArgs == 0 || HasTemplateKWAndArgsInfo);
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, NumTemplateArgs);
  void *Mem = C.Allocate(Size);
  auto *E = new (Mem) DependentScopeDeclRefExpr(
      QualType(), NestedNameSpecifierLoc(), SourceLocation(),
      DeclarationNameInfo(), nullptr);
  E->HasTemplateKWAndArgsInfo = HasTemplateKWAndArgsInfo;
  return E;
}

SourceLocation CXXConstructExpr::getBeginLoc() const {
  if (isa<CXXTemporaryObjectExpr>(this))
    return cast<CXXTemporaryObjectExpr>(this)->getBeginLoc();
  return getLocation();
}

SourceLocation CXXConstructExpr::getEndLoc() const {
  if (isa<CXXTemporaryObjectExpr>(this))
    return cast<CXXTemporaryObjectExpr>(this)->getEndLoc();

  if (ParenOrBraceRange.isValid())
    return ParenOrBraceRange.getEnd();

  SourceLocation End = getLocation();
  for (unsigned I = getNumArgs(); I > 0; --I) {
    const Expr *Arg = getArg(I - 1);
    if (!Arg->isDefaultArgument()) {
      SourceLocation NewEnd = Arg->getEndLoc();
      if (NewEnd.isValid()) {
        End = NewEnd;
        break;
      }
    }
  }

  return End;
}

CXXOperatorCallExpr::CXXOperatorCallExpr(OverloadedOperatorKind OpKind,
                                         Expr *Fn, ArrayRef<Expr *> Args,
                                         QualType Ty, ExprValueKind VK,
                                         SourceLocation OperatorLoc,
                                         FPOptions FPFeatures,
                                         ADLCallKind UsesADL)
    : CallExpr(CXXOperatorCallExprClass, Fn, /*PreArgs=*/{}, Args, Ty, VK,
               OperatorLoc, /*MinNumArgs=*/0, UsesADL) {
  CXXOperatorCallExprBits.OperatorKind = OpKind;
  CXXOperatorCallExprBits.FPFeatures = FPFeatures.getInt();
  assert(
      (CXXOperatorCallExprBits.OperatorKind == static_cast<unsigned>(OpKind)) &&
      "OperatorKind overflow!");
  assert((CXXOperatorCallExprBits.FPFeatures == FPFeatures.getInt()) &&
         "FPFeatures overflow!");
  Range = getSourceRangeImpl();
}

CXXOperatorCallExpr::CXXOperatorCallExpr(unsigned NumArgs, EmptyShell Empty)
    : CallExpr(CXXOperatorCallExprClass, /*NumPreArgs=*/0, NumArgs, Empty) {}

CXXOperatorCallExpr *CXXOperatorCallExpr::Create(
    const ASTContext &Ctx, OverloadedOperatorKind OpKind, Expr *Fn,
    ArrayRef<Expr *> Args, QualType Ty, ExprValueKind VK,
    SourceLocation OperatorLoc, FPOptions FPFeatures, ADLCallKind UsesADL) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned NumArgs = Args.size();
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CXXOperatorCallExpr) + SizeOfTrailingObjects,
                           alignof(CXXOperatorCallExpr));
  return new (Mem) CXXOperatorCallExpr(OpKind, Fn, Args, Ty, VK, OperatorLoc,
                                       FPFeatures, UsesADL);
}

CXXOperatorCallExpr *CXXOperatorCallExpr::CreateEmpty(const ASTContext &Ctx,
                                                      unsigned NumArgs,
                                                      EmptyShell Empty) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CXXOperatorCallExpr) + SizeOfTrailingObjects,
                           alignof(CXXOperatorCallExpr));
  return new (Mem) CXXOperatorCallExpr(NumArgs, Empty);
}

SourceRange CXXOperatorCallExpr::getSourceRangeImpl() const {
  OverloadedOperatorKind Kind = getOperator();
  if (Kind == OO_PlusPlus || Kind == OO_MinusMinus) {
    if (getNumArgs() == 1)
      // Prefix operator
      return SourceRange(getOperatorLoc(), getArg(0)->getEndLoc());
    else
      // Postfix operator
      return SourceRange(getArg(0)->getBeginLoc(), getOperatorLoc());
  } else if (Kind == OO_Arrow) {
    return getArg(0)->getSourceRange();
  } else if (Kind == OO_Call) {
    return SourceRange(getArg(0)->getBeginLoc(), getRParenLoc());
  } else if (Kind == OO_Subscript) {
    return SourceRange(getArg(0)->getBeginLoc(), getRParenLoc());
  } else if (getNumArgs() == 1) {
    return SourceRange(getOperatorLoc(), getArg(0)->getEndLoc());
  } else if (getNumArgs() == 2) {
    return SourceRange(getArg(0)->getBeginLoc(), getArg(1)->getEndLoc());
  } else {
    return getOperatorLoc();
  }
}

CXXMemberCallExpr::CXXMemberCallExpr(Expr *Fn, ArrayRef<Expr *> Args,
                                     QualType Ty, ExprValueKind VK,
                                     SourceLocation RP, unsigned MinNumArgs)
    : CallExpr(CXXMemberCallExprClass, Fn, /*PreArgs=*/{}, Args, Ty, VK, RP,
               MinNumArgs, NotADL) {}

CXXMemberCallExpr::CXXMemberCallExpr(unsigned NumArgs, EmptyShell Empty)
    : CallExpr(CXXMemberCallExprClass, /*NumPreArgs=*/0, NumArgs, Empty) {}

CXXMemberCallExpr *CXXMemberCallExpr::Create(const ASTContext &Ctx, Expr *Fn,
                                             ArrayRef<Expr *> Args, QualType Ty,
                                             ExprValueKind VK,
                                             SourceLocation RP,
                                             unsigned MinNumArgs) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned NumArgs = std::max<unsigned>(Args.size(), MinNumArgs);
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CXXMemberCallExpr) + SizeOfTrailingObjects,
                           alignof(CXXMemberCallExpr));
  return new (Mem) CXXMemberCallExpr(Fn, Args, Ty, VK, RP, MinNumArgs);
}

CXXMemberCallExpr *CXXMemberCallExpr::CreateEmpty(const ASTContext &Ctx,
                                                  unsigned NumArgs,
                                                  EmptyShell Empty) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CXXMemberCallExpr) + SizeOfTrailingObjects,
                           alignof(CXXMemberCallExpr));
  return new (Mem) CXXMemberCallExpr(NumArgs, Empty);
}

Expr *CXXMemberCallExpr::getImplicitObjectArgument() const {
  const Expr *Callee = getCallee()->IgnoreParens();
  if (const auto *MemExpr = dyn_cast<MemberExpr>(Callee))
    return MemExpr->getBase();
  if (const auto *BO = dyn_cast<BinaryOperator>(Callee))
    if (BO->getOpcode() == BO_PtrMemD || BO->getOpcode() == BO_PtrMemI)
      return BO->getLHS();

  // FIXME: Will eventually need to cope with member pointers.
  return nullptr;
}

CXXMethodDecl *CXXMemberCallExpr::getMethodDecl() const {
  if (const auto *MemExpr = dyn_cast<MemberExpr>(getCallee()->IgnoreParens()))
    return cast<CXXMethodDecl>(MemExpr->getMemberDecl());

  // FIXME: Will eventually need to cope with member pointers.
  return nullptr;
}

CXXRecordDecl *CXXMemberCallExpr::getRecordDecl() const {
  Expr *ThisArg = getImplicitObjectArgument();
  if (!ThisArg)
    return nullptr;

  if (ThisArg->getType()->isAnyPointerType())
    return ThisArg->getType()->getPointeeType()->getAsCXXRecordDecl();

  return ThisArg->getType()->getAsCXXRecordDecl();
}

//===----------------------------------------------------------------------===//
//  Named casts
//===----------------------------------------------------------------------===//

/// getCastName - Get the name of the C++ cast being used, e.g.,
/// "static_cast", "dynamic_cast", "reinterpret_cast", or
/// "const_cast". The returned pointer must not be freed.
const char *CXXNamedCastExpr::getCastName() const {
  switch (getStmtClass()) {
  case CXXStaticCastExprClass:
    return "static_cast";
  case CXXDynamicCastExprClass:
    return "dynamic_cast";
  case CXXReinterpretCastExprClass:
    return "reinterpret_cast";
  case CXXConstCastExprClass:
    return "const_cast";
  default:
    return "<invalid cast>";
  }
}

CXXStaticCastExpr *
CXXStaticCastExpr::Create(const ASTContext &C, QualType T, ExprValueKind VK,
                          CastKind K, Expr *Op, const CXXCastPath *BasePath,
                          TypeSourceInfo *WrittenTy, SourceLocation L,
                          SourceLocation RParenLoc, SourceRange AngleBrackets) {
  unsigned PathSize = (BasePath ? BasePath->size() : 0);
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  auto *E = new (Buffer) CXXStaticCastExpr(T, VK, K, Op, PathSize, WrittenTy, L,
                                           RParenLoc, AngleBrackets);
  if (PathSize)
    std::uninitialized_copy_n(BasePath->data(), BasePath->size(),
                              E->getTrailingObjects<CXXBaseSpecifier *>());
  return E;
}

CXXStaticCastExpr *CXXStaticCastExpr::CreateEmpty(const ASTContext &C,
                                                  unsigned PathSize) {
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  return new (Buffer) CXXStaticCastExpr(EmptyShell(), PathSize);
}

CXXDynamicCastExpr *CXXDynamicCastExpr::Create(
    const ASTContext &C, QualType T, ExprValueKind VK, CastKind K, Expr *Op,
    const CXXCastPath *BasePath, TypeSourceInfo *WrittenTy, SourceLocation L,
    SourceLocation RParenLoc, SourceRange AngleBrackets) {
  unsigned PathSize = (BasePath ? BasePath->size() : 0);
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  auto *E = new (Buffer) CXXDynamicCastExpr(T, VK, K, Op, PathSize, WrittenTy,
                                            L, RParenLoc, AngleBrackets);
  if (PathSize)
    std::uninitialized_copy_n(BasePath->data(), BasePath->size(),
                              E->getTrailingObjects<CXXBaseSpecifier *>());
  return E;
}

CXXDynamicCastExpr *CXXDynamicCastExpr::CreateEmpty(const ASTContext &C,
                                                    unsigned PathSize) {
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  return new (Buffer) CXXDynamicCastExpr(EmptyShell(), PathSize);
}

/// isAlwaysNull - Return whether the result of the dynamic_cast is proven
/// to always be null. For example:
///
/// struct A { };
/// struct B final : A { };
/// struct C { };
///
/// C *f(B* b) { return dynamic_cast<C*>(b); }
bool CXXDynamicCastExpr::isAlwaysNull() const {
  QualType SrcType = getSubExpr()->getType();
  QualType DestType = getType();

  if (const auto *SrcPTy = SrcType->getAs<PointerType>()) {
    SrcType = SrcPTy->getPointeeType();
    DestType = DestType->castAs<PointerType>()->getPointeeType();
  }

  if (DestType->isVoidType())
    return false;

  const auto *SrcRD =
      cast<CXXRecordDecl>(SrcType->castAs<RecordType>()->getDecl());

  if (!SrcRD->hasAttr<FinalAttr>())
    return false;

  const auto *DestRD =
      cast<CXXRecordDecl>(DestType->castAs<RecordType>()->getDecl());

  return !DestRD->isDerivedFrom(SrcRD);
}

CXXReinterpretCastExpr *CXXReinterpretCastExpr::Create(
    const ASTContext &C, QualType T, ExprValueKind VK, CastKind K, Expr *Op,
    const CXXCastPath *BasePath, TypeSourceInfo *WrittenTy, SourceLocation L,
    SourceLocation RParenLoc, SourceRange AngleBrackets) {
  unsigned PathSize = (BasePath ? BasePath->size() : 0);
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  auto *E = new (Buffer) CXXReinterpretCastExpr(
      T, VK, K, Op, PathSize, WrittenTy, L, RParenLoc, AngleBrackets);
  if (PathSize)
    std::uninitialized_copy_n(BasePath->data(), BasePath->size(),
                              E->getTrailingObjects<CXXBaseSpecifier *>());
  return E;
}

CXXReinterpretCastExpr *CXXReinterpretCastExpr::CreateEmpty(const ASTContext &C,
                                                            unsigned PathSize) {
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  return new (Buffer) CXXReinterpretCastExpr(EmptyShell(), PathSize);
}

CXXConstCastExpr *
CXXConstCastExpr::Create(const ASTContext &C, QualType T, ExprValueKind VK,
                         Expr *Op, TypeSourceInfo *WrittenTy, SourceLocation L,
                         SourceLocation RParenLoc, SourceRange AngleBrackets) {
  return new (C)
      CXXConstCastExpr(T, VK, Op, WrittenTy, L, RParenLoc, AngleBrackets);
}

CXXConstCastExpr *CXXConstCastExpr::CreateEmpty(const ASTContext &C) {
  return new (C) CXXConstCastExpr(EmptyShell());
}

CXXFunctionalCastExpr *
CXXFunctionalCastExpr::Create(const ASTContext &C, QualType T, ExprValueKind VK,
                              TypeSourceInfo *Written, CastKind K, Expr *Op,
                              const CXXCastPath *BasePath, SourceLocation L,
                              SourceLocation R) {
  unsigned PathSize = (BasePath ? BasePath->size() : 0);
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  auto *E =
      new (Buffer) CXXFunctionalCastExpr(T, VK, Written, K, Op, PathSize, L, R);
  if (PathSize)
    std::uninitialized_copy_n(BasePath->data(), BasePath->size(),
                              E->getTrailingObjects<CXXBaseSpecifier *>());
  return E;
}

CXXFunctionalCastExpr *CXXFunctionalCastExpr::CreateEmpty(const ASTContext &C,
                                                          unsigned PathSize) {
  void *Buffer =
      C.Allocate(totalSizeToAlloc<CastExpr::BasePathSizeTy, CXXBaseSpecifier *>(
          PathSize ? 1 : 0, PathSize));
  return new (Buffer) CXXFunctionalCastExpr(EmptyShell(), PathSize);
}

SourceLocation CXXFunctionalCastExpr::getBeginLoc() const {
  return getTypeInfoAsWritten()->getTypeLoc().getBeginLoc();
}

SourceLocation CXXFunctionalCastExpr::getEndLoc() const {
  return RParenLoc.isValid() ? RParenLoc : getSubExpr()->getEndLoc();
}

UserDefinedLiteral::UserDefinedLiteral(Expr *Fn, ArrayRef<Expr *> Args,
                                       QualType Ty, ExprValueKind VK,
                                       SourceLocation LitEndLoc,
                                       SourceLocation SuffixLoc)
    : CallExpr(UserDefinedLiteralClass, Fn, /*PreArgs=*/{}, Args, Ty, VK,
               LitEndLoc, /*MinNumArgs=*/0, NotADL),
      UDSuffixLoc(SuffixLoc) {}

UserDefinedLiteral::UserDefinedLiteral(unsigned NumArgs, EmptyShell Empty)
    : CallExpr(UserDefinedLiteralClass, /*NumPreArgs=*/0, NumArgs, Empty) {}

UserDefinedLiteral *UserDefinedLiteral::Create(const ASTContext &Ctx, Expr *Fn,
                                               ArrayRef<Expr *> Args,
                                               QualType Ty, ExprValueKind VK,
                                               SourceLocation LitEndLoc,
                                               SourceLocation SuffixLoc) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned NumArgs = Args.size();
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(UserDefinedLiteral) + SizeOfTrailingObjects,
                           alignof(UserDefinedLiteral));
  return new (Mem) UserDefinedLiteral(Fn, Args, Ty, VK, LitEndLoc, SuffixLoc);
}

UserDefinedLiteral *UserDefinedLiteral::CreateEmpty(const ASTContext &Ctx,
                                                    unsigned NumArgs,
                                                    EmptyShell Empty) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/0, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(UserDefinedLiteral) + SizeOfTrailingObjects,
                           alignof(UserDefinedLiteral));
  return new (Mem) UserDefinedLiteral(NumArgs, Empty);
}

UserDefinedLiteral::LiteralOperatorKind
UserDefinedLiteral::getLiteralOperatorKind() const {
  if (getNumArgs() == 0)
    return LOK_Template;
  if (getNumArgs() == 2)
    return LOK_String;

  assert(getNumArgs() == 1 && "unexpected #args in literal operator call");
  QualType ParamTy =
      cast<FunctionDecl>(getCalleeDecl())->getParamDecl(0)->getType();
  if (ParamTy->isPointerType())
    return LOK_Raw;
  if (ParamTy->isAnyCharacterType())
    return LOK_Character;
  if (ParamTy->isIntegerType())
    return LOK_Integer;
  if (ParamTy->isFloatingType())
    return LOK_Floating;

  llvm_unreachable("unknown kind of literal operator");
}

Expr *UserDefinedLiteral::getCookedLiteral() {
#ifndef NDEBUG
  LiteralOperatorKind LOK = getLiteralOperatorKind();
  assert(LOK != LOK_Template && LOK != LOK_Raw && "not a cooked literal");
#endif
  return getArg(0);
}

const IdentifierInfo *UserDefinedLiteral::getUDSuffix() const {
  return cast<FunctionDecl>(getCalleeDecl())->getLiteralIdentifier();
}

CXXDefaultInitExpr::CXXDefaultInitExpr(const ASTContext &Ctx,
                                       SourceLocation Loc, FieldDecl *Field,
                                       QualType Ty)
    : Expr(CXXDefaultInitExprClass, Ty.getNonLValueExprType(Ctx),
           Ty->isLValueReferenceType()
               ? VK_LValue
               : Ty->isRValueReferenceType() ? VK_XValue : VK_RValue,
           /*FIXME*/ OK_Ordinary, false, false, false, false),
      Field(Field) {
  CXXDefaultInitExprBits.Loc = Loc;
  assert(Field->hasInClassInitializer());
}

CXXTemporary *CXXTemporary::Create(const ASTContext &C,
                                   const CXXDestructorDecl *Destructor) {
  return new (C) CXXTemporary(Destructor);
}

CXXBindTemporaryExpr *CXXBindTemporaryExpr::Create(const ASTContext &C,
                                                   CXXTemporary *Temp,
                                                   Expr *SubExpr) {
  assert((SubExpr->getType()->isRecordType() ||
          SubExpr->getType()->isArrayType()) &&
         "Expression bound to a temporary must have record or array type!");

  return new (C) CXXBindTemporaryExpr(Temp, SubExpr);
}

CXXTemporaryObjectExpr::CXXTemporaryObjectExpr(
    CXXConstructorDecl *Cons, QualType Ty, TypeSourceInfo *TSI,
    ArrayRef<Expr *> Args, SourceRange ParenOrBraceRange,
    bool HadMultipleCandidates, bool ListInitialization,
    bool StdInitListInitialization, bool ZeroInitialization)
    : CXXConstructExpr(
          CXXTemporaryObjectExprClass, Ty, TSI->getTypeLoc().getBeginLoc(),
          Cons, /* Elidable=*/false, Args, HadMultipleCandidates,
          ListInitialization, StdInitListInitialization, ZeroInitialization,
          CXXConstructExpr::CK_Complete, ParenOrBraceRange),
      TSI(TSI) {}

CXXTemporaryObjectExpr::CXXTemporaryObjectExpr(EmptyShell Empty,
                                               unsigned NumArgs)
    : CXXConstructExpr(CXXTemporaryObjectExprClass, Empty, NumArgs) {}

CXXTemporaryObjectExpr *CXXTemporaryObjectExpr::Create(
    const ASTContext &Ctx, CXXConstructorDecl *Cons, QualType Ty,
    TypeSourceInfo *TSI, ArrayRef<Expr *> Args, SourceRange ParenOrBraceRange,
    bool HadMultipleCandidates, bool ListInitialization,
    bool StdInitListInitialization, bool ZeroInitialization) {
  unsigned SizeOfTrailingObjects = sizeOfTrailingObjects(Args.size());
  void *Mem =
      Ctx.Allocate(sizeof(CXXTemporaryObjectExpr) + SizeOfTrailingObjects,
                   alignof(CXXTemporaryObjectExpr));
  return new (Mem) CXXTemporaryObjectExpr(
      Cons, Ty, TSI, Args, ParenOrBraceRange, HadMultipleCandidates,
      ListInitialization, StdInitListInitialization, ZeroInitialization);
}

CXXTemporaryObjectExpr *
CXXTemporaryObjectExpr::CreateEmpty(const ASTContext &Ctx, unsigned NumArgs) {
  unsigned SizeOfTrailingObjects = sizeOfTrailingObjects(NumArgs);
  void *Mem =
      Ctx.Allocate(sizeof(CXXTemporaryObjectExpr) + SizeOfTrailingObjects,
                   alignof(CXXTemporaryObjectExpr));
  return new (Mem) CXXTemporaryObjectExpr(EmptyShell(), NumArgs);
}

SourceLocation CXXTemporaryObjectExpr::getBeginLoc() const {
  return getTypeSourceInfo()->getTypeLoc().getBeginLoc();
}

SourceLocation CXXTemporaryObjectExpr::getEndLoc() const {
  SourceLocation Loc = getParenOrBraceRange().getEnd();
  if (Loc.isInvalid() && getNumArgs())
    Loc = getArg(getNumArgs() - 1)->getEndLoc();
  return Loc;
}

CXXConstructExpr *CXXConstructExpr::Create(
    const ASTContext &Ctx, QualType Ty, SourceLocation Loc,
    CXXConstructorDecl *Ctor, bool Elidable, ArrayRef<Expr *> Args,
    bool HadMultipleCandidates, bool ListInitialization,
    bool StdInitListInitialization, bool ZeroInitialization,
    ConstructionKind ConstructKind, SourceRange ParenOrBraceRange) {
  unsigned SizeOfTrailingObjects = sizeOfTrailingObjects(Args.size());
  void *Mem = Ctx.Allocate(sizeof(CXXConstructExpr) + SizeOfTrailingObjects,
                           alignof(CXXConstructExpr));
  return new (Mem) CXXConstructExpr(
      CXXConstructExprClass, Ty, Loc, Ctor, Elidable, Args,
      HadMultipleCandidates, ListInitialization, StdInitListInitialization,
      ZeroInitialization, ConstructKind, ParenOrBraceRange);
}

CXXConstructExpr *CXXConstructExpr::CreateEmpty(const ASTContext &Ctx,
                                                unsigned NumArgs) {
  unsigned SizeOfTrailingObjects = sizeOfTrailingObjects(NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CXXConstructExpr) + SizeOfTrailingObjects,
                           alignof(CXXConstructExpr));
  return new (Mem)
      CXXConstructExpr(CXXConstructExprClass, EmptyShell(), NumArgs);
}

CXXConstructExpr::CXXConstructExpr(
    StmtClass SC, QualType Ty, SourceLocation Loc, CXXConstructorDecl *Ctor,
    bool Elidable, ArrayRef<Expr *> Args, bool HadMultipleCandidates,
    bool ListInitialization, bool StdInitListInitialization,
    bool ZeroInitialization, ConstructionKind ConstructKind,
    SourceRange ParenOrBraceRange)
    : Expr(SC, Ty, VK_RValue, OK_Ordinary, Ty->isDependentType(),
           Ty->isDependentType(), Ty->isInstantiationDependentType(),
           Ty->containsUnexpandedParameterPack()),
      Constructor(Ctor), ParenOrBraceRange(ParenOrBraceRange),
      NumArgs(Args.size()) {
  CXXConstructExprBits.Elidable = Elidable;
  CXXConstructExprBits.HadMultipleCandidates = HadMultipleCandidates;
  CXXConstructExprBits.ListInitialization = ListInitialization;
  CXXConstructExprBits.StdInitListInitialization = StdInitListInitialization;
  CXXConstructExprBits.ZeroInitialization = ZeroInitialization;
  CXXConstructExprBits.ConstructionKind = ConstructKind;
  CXXConstructExprBits.Loc = Loc;

  Stmt **TrailingArgs = getTrailingArgs();
  for (unsigned I = 0, N = Args.size(); I != N; ++I) {
    assert(Args[I] && "NULL argument in CXXConstructExpr!");

    if (Args[I]->isValueDependent())
      ExprBits.ValueDependent = true;
    if (Args[I]->isInstantiationDependent())
      ExprBits.InstantiationDependent = true;
    if (Args[I]->containsUnexpandedParameterPack())
      ExprBits.ContainsUnexpandedParameterPack = true;

    TrailingArgs[I] = Args[I];
  }
}

CXXConstructExpr::CXXConstructExpr(StmtClass SC, EmptyShell Empty,
                                   unsigned NumArgs)
    : Expr(SC, Empty), NumArgs(NumArgs) {}

LambdaCapture::LambdaCapture(SourceLocation Loc, bool Implicit,
                             LambdaCaptureKind Kind, VarDecl *Var,
                             SourceLocation EllipsisLoc)
    : DeclAndBits(Var, 0), Loc(Loc), EllipsisLoc(EllipsisLoc) {
  unsigned Bits = 0;
  if (Implicit)
    Bits |= Capture_Implicit;

  switch (Kind) {
  case LCK_StarThis:
    Bits |= Capture_ByCopy;
    LLVM_FALLTHROUGH;
  case LCK_This:
    assert(!Var && "'this' capture cannot have a variable!");
    Bits |= Capture_This;
    break;

  case LCK_ByCopy:
    Bits |= Capture_ByCopy;
    LLVM_FALLTHROUGH;
  case LCK_ByRef:
    assert(Var && "capture must have a variable!");
    break;
  case LCK_VLAType:
    assert(!Var && "VLA type capture cannot have a variable!");
    break;
  }
  DeclAndBits.setInt(Bits);
}

LambdaCaptureKind LambdaCapture::getCaptureKind() const {
  if (capturesVLAType())
    return LCK_VLAType;
  bool CapByCopy = DeclAndBits.getInt() & Capture_ByCopy;
  if (capturesThis())
    return CapByCopy ? LCK_StarThis : LCK_This;
  return CapByCopy ? LCK_ByCopy : LCK_ByRef;
}

LambdaExpr::LambdaExpr(QualType T, SourceRange IntroducerRange,
                       LambdaCaptureDefault CaptureDefault,
                       SourceLocation CaptureDefaultLoc,
                       ArrayRef<LambdaCapture> Captures, bool ExplicitParams,
                       bool ExplicitResultType, ArrayRef<Expr *> CaptureInits,
                       SourceLocation ClosingBrace,
                       bool ContainsUnexpandedParameterPack)
    : Expr(LambdaExprClass, T, VK_RValue, OK_Ordinary, T->isDependentType(),
           T->isDependentType(), T->isDependentType(),
           ContainsUnexpandedParameterPack),
      IntroducerRange(IntroducerRange), CaptureDefaultLoc(CaptureDefaultLoc),
      NumCaptures(Captures.size()), CaptureDefault(CaptureDefault),
      ExplicitParams(ExplicitParams), ExplicitResultType(ExplicitResultType),
      ClosingBrace(ClosingBrace) {
  assert(CaptureInits.size() == Captures.size() && "Wrong number of arguments");
  CXXRecordDecl *Class = getLambdaClass();
  CXXRecordDecl::LambdaDefinitionData &Data = Class->getLambdaData();

  // FIXME: Propagate "has unexpanded parameter pack" bit.

  // Copy captures.
  const ASTContext &Context = Class->getASTContext();
  Data.NumCaptures = NumCaptures;
  Data.NumExplicitCaptures = 0;
  Data.Captures =
      (LambdaCapture *)Context.Allocate(sizeof(LambdaCapture) * NumCaptures);
  LambdaCapture *ToCapture = Data.Captures;
  for (unsigned I = 0, N = Captures.size(); I != N; ++I) {
    if (Captures[I].isExplicit())
      ++Data.NumExplicitCaptures;

    *ToCapture++ = Captures[I];
  }

  // Copy initialization expressions for the non-static data members.
  Stmt **Stored = getStoredStmts();
  for (unsigned I = 0, N = CaptureInits.size(); I != N; ++I)
    *Stored++ = CaptureInits[I];

  // Copy the body of the lambda.
  *Stored++ = getCallOperator()->getBody();
}

LambdaExpr *LambdaExpr::Create(
    const ASTContext &Context, CXXRecordDecl *Class,
    SourceRange IntroducerRange, LambdaCaptureDefault CaptureDefault,
    SourceLocation CaptureDefaultLoc, ArrayRef<LambdaCapture> Captures,
    bool ExplicitParams, bool ExplicitResultType, ArrayRef<Expr *> CaptureInits,
    SourceLocation ClosingBrace, bool ContainsUnexpandedParameterPack) {
  // Determine the type of the expression (i.e., the type of the
  // function object we're creating).
  QualType T = Context.getTypeDeclType(Class);

  unsigned Size = totalSizeToAlloc<Stmt *>(Captures.size() + 1);
  void *Mem = Context.Allocate(Size);
  return new (Mem)
      LambdaExpr(T, IntroducerRange, CaptureDefault, CaptureDefaultLoc,
                 Captures, ExplicitParams, ExplicitResultType, CaptureInits,
                 ClosingBrace, ContainsUnexpandedParameterPack);
}

LambdaExpr *LambdaExpr::CreateDeserialized(const ASTContext &C,
                                           unsigned NumCaptures) {
  unsigned Size = totalSizeToAlloc<Stmt *>(NumCaptures + 1);
  void *Mem = C.Allocate(Size);
  return new (Mem) LambdaExpr(EmptyShell(), NumCaptures);
}

bool LambdaExpr::isInitCapture(const LambdaCapture *C) const {
  return (C->capturesVariable() && C->getCapturedVar()->isInitCapture() &&
          (getCallOperator() == C->getCapturedVar()->getDeclContext()));
}

LambdaExpr::capture_iterator LambdaExpr::capture_begin() const {
  return getLambdaClass()->getLambdaData().Captures;
}

LambdaExpr::capture_iterator LambdaExpr::capture_end() const {
  return capture_begin() + NumCaptures;
}

LambdaExpr::capture_range LambdaExpr::captures() const {
  return capture_range(capture_begin(), capture_end());
}

LambdaExpr::capture_iterator LambdaExpr::explicit_capture_begin() const {
  return capture_begin();
}

LambdaExpr::capture_iterator LambdaExpr::explicit_capture_end() const {
  struct CXXRecordDecl::LambdaDefinitionData &Data =
      getLambdaClass()->getLambdaData();
  return Data.Captures + Data.NumExplicitCaptures;
}

LambdaExpr::capture_range LambdaExpr::explicit_captures() const {
  return capture_range(explicit_capture_begin(), explicit_capture_end());
}

LambdaExpr::capture_iterator LambdaExpr::implicit_capture_begin() const {
  return explicit_capture_end();
}

LambdaExpr::capture_iterator LambdaExpr::implicit_capture_end() const {
  return capture_end();
}

LambdaExpr::capture_range LambdaExpr::implicit_captures() const {
  return capture_range(implicit_capture_begin(), implicit_capture_end());
}

CXXRecordDecl *LambdaExpr::getLambdaClass() const {
  return getType()->getAsCXXRecordDecl();
}

CXXMethodDecl *LambdaExpr::getCallOperator() const {
  CXXRecordDecl *Record = getLambdaClass();
  return Record->getLambdaCallOperator();
}

TemplateParameterList *LambdaExpr::getTemplateParameterList() const {
  CXXRecordDecl *Record = getLambdaClass();
  return Record->getGenericLambdaTemplateParameterList();
}

CompoundStmt *LambdaExpr::getBody() const {
  // FIXME: this mutation in getBody is bogus. It should be
  // initialized in ASTStmtReader::VisitLambdaExpr, but for reasons I
  // don't understand, that doesn't work.
  if (!getStoredStmts()[NumCaptures])
    *const_cast<Stmt **>(&getStoredStmts()[NumCaptures]) =
        getCallOperator()->getBody();

  return static_cast<CompoundStmt *>(getStoredStmts()[NumCaptures]);
}

bool LambdaExpr::isMutable() const { return !getCallOperator()->isConst(); }

ExprWithCleanups::ExprWithCleanups(Expr *subexpr, bool CleanupsHaveSideEffects,
                                   ArrayRef<CleanupObject> objects)
    : FullExpr(ExprWithCleanupsClass, subexpr) {
  ExprWithCleanupsBits.CleanupsHaveSideEffects = CleanupsHaveSideEffects;
  ExprWithCleanupsBits.NumObjects = objects.size();
  for (unsigned i = 0, e = objects.size(); i != e; ++i)
    getTrailingObjects<CleanupObject>()[i] = objects[i];
}

ExprWithCleanups *ExprWithCleanups::Create(const ASTContext &C, Expr *subexpr,
                                           bool CleanupsHaveSideEffects,
                                           ArrayRef<CleanupObject> objects) {
  void *buffer = C.Allocate(totalSizeToAlloc<CleanupObject>(objects.size()),
                            alignof(ExprWithCleanups));
  return new (buffer)
      ExprWithCleanups(subexpr, CleanupsHaveSideEffects, objects);
}

ExprWithCleanups::ExprWithCleanups(EmptyShell empty, unsigned numObjects)
    : FullExpr(ExprWithCleanupsClass, empty) {
  ExprWithCleanupsBits.NumObjects = numObjects;
}

ExprWithCleanups *ExprWithCleanups::Create(const ASTContext &C,
                                           EmptyShell empty,
                                           unsigned numObjects) {
  void *buffer = C.Allocate(totalSizeToAlloc<CleanupObject>(numObjects),
                            alignof(ExprWithCleanups));
  return new (buffer) ExprWithCleanups(empty, numObjects);
}

CXXUnresolvedConstructExpr::CXXUnresolvedConstructExpr(TypeSourceInfo *Type,
                                                       SourceLocation LParenLoc,
                                                       ArrayRef<Expr *> Args,
                                                       SourceLocation RParenLoc)
    : Expr(CXXUnresolvedConstructExprClass,
           Type->getType().getNonReferenceType(),
           (Type->getType()->isLValueReferenceType()
                ? VK_LValue
                : Type->getType()->isRValueReferenceType() ? VK_XValue
                                                           : VK_RValue),
           OK_Ordinary,
           Type->getType()->isDependentType() ||
               Type->getType()->getContainedDeducedType(),
           true, true, Type->getType()->containsUnexpandedParameterPack()),
      Type(Type), LParenLoc(LParenLoc), RParenLoc(RParenLoc),
      NumArgs(Args.size()) {
  auto **StoredArgs = getTrailingObjects<Expr *>();
  for (unsigned I = 0; I != Args.size(); ++I) {
    if (Args[I]->containsUnexpandedParameterPack())
      ExprBits.ContainsUnexpandedParameterPack = true;

    StoredArgs[I] = Args[I];
  }
}

CXXUnresolvedConstructExpr *CXXUnresolvedConstructExpr::Create(
    const ASTContext &C, TypeSourceInfo *Type, SourceLocation LParenLoc,
    ArrayRef<Expr *> Args, SourceLocation RParenLoc) {
  void *Mem = C.Allocate(totalSizeToAlloc<Expr *>(Args.size()));
  return new (Mem) CXXUnresolvedConstructExpr(Type, LParenLoc, Args, RParenLoc);
}

CXXUnresolvedConstructExpr *
CXXUnresolvedConstructExpr::CreateEmpty(const ASTContext &C, unsigned NumArgs) {
  Stmt::EmptyShell Empty;
  void *Mem = C.Allocate(totalSizeToAlloc<Expr *>(NumArgs));
  return new (Mem) CXXUnresolvedConstructExpr(Empty, NumArgs);
}

SourceLocation CXXUnresolvedConstructExpr::getBeginLoc() const {
  return Type->getTypeLoc().getBeginLoc();
}

CXXDependentScopeMemberExpr::CXXDependentScopeMemberExpr(
    const ASTContext &C, Expr *Base, QualType BaseType, bool IsArrow,
    SourceLocation OperatorLoc, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateKWLoc, NamedDecl *FirstQualifierFoundInScope,
    DeclarationNameInfo MemberNameInfo,
    const TemplateArgumentListInfo *TemplateArgs)
    : Expr(CXXDependentScopeMemberExprClass, C.DependentTy, VK_LValue,
           OK_Ordinary, true, true, true,
           ((Base && Base->containsUnexpandedParameterPack()) ||
            (QualifierLoc && QualifierLoc.getNestedNameSpecifier()
                                 ->containsUnexpandedParameterPack()) ||
            MemberNameInfo.containsUnexpandedParameterPack())),
      Base(Base), BaseType(BaseType), IsArrow(IsArrow),
      HasTemplateKWAndArgsInfo(TemplateArgs != nullptr ||
                               TemplateKWLoc.isValid()),
      OperatorLoc(OperatorLoc), QualifierLoc(QualifierLoc),
      FirstQualifierFoundInScope(FirstQualifierFoundInScope),
      MemberNameInfo(MemberNameInfo) {
  if (TemplateArgs) {
    bool Dependent = true;
    bool InstantiationDependent = true;
    bool ContainsUnexpandedParameterPack = false;
    getTrailingObjects<ASTTemplateKWAndArgsInfo>()->initializeFrom(
        TemplateKWLoc, *TemplateArgs, getTrailingObjects<TemplateArgumentLoc>(),
        Dependent, InstantiationDependent, ContainsUnexpandedParameterPack);
    if (ContainsUnexpandedParameterPack)
      ExprBits.ContainsUnexpandedParameterPack = true;
  } else if (TemplateKWLoc.isValid()) {
    getTrailingObjects<ASTTemplateKWAndArgsInfo>()->initializeFrom(
        TemplateKWLoc);
  }
}

CXXDependentScopeMemberExpr *CXXDependentScopeMemberExpr::Create(
    const ASTContext &C, Expr *Base, QualType BaseType, bool IsArrow,
    SourceLocation OperatorLoc, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateKWLoc, NamedDecl *FirstQualifierFoundInScope,
    DeclarationNameInfo MemberNameInfo,
    const TemplateArgumentListInfo *TemplateArgs) {
  bool HasTemplateKWAndArgsInfo = TemplateArgs || TemplateKWLoc.isValid();
  unsigned NumTemplateArgs = TemplateArgs ? TemplateArgs->size() : 0;
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, NumTemplateArgs);

  void *Mem = C.Allocate(Size, alignof(CXXDependentScopeMemberExpr));
  return new (Mem) CXXDependentScopeMemberExpr(
      C, Base, BaseType, IsArrow, OperatorLoc, QualifierLoc, TemplateKWLoc,
      FirstQualifierFoundInScope, MemberNameInfo, TemplateArgs);
}

CXXDependentScopeMemberExpr *
CXXDependentScopeMemberExpr::CreateEmpty(const ASTContext &C,
                                         bool HasTemplateKWAndArgsInfo,
                                         unsigned NumTemplateArgs) {
  assert(NumTemplateArgs == 0 || HasTemplateKWAndArgsInfo);
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, NumTemplateArgs);
  void *Mem = C.Allocate(Size, alignof(CXXDependentScopeMemberExpr));
  auto *E = new (Mem) CXXDependentScopeMemberExpr(
      C, nullptr, QualType(), false, SourceLocation(), NestedNameSpecifierLoc(),
      SourceLocation(), nullptr, DeclarationNameInfo(), nullptr);
  E->HasTemplateKWAndArgsInfo = HasTemplateKWAndArgsInfo;
  return E;
}

bool CXXDependentScopeMemberExpr::isImplicitAccess() const {
  if (!Base)
    return true;

  return cast<Expr>(Base)->isImplicitCXXThis();
}

static bool hasOnlyNonStaticMemberFunctions(UnresolvedSetIterator begin,
                                            UnresolvedSetIterator end) {
  do {
    NamedDecl *decl = *begin;
    if (isa<UnresolvedUsingValueDecl>(decl))
      return false;

    // Unresolved member expressions should only contain methods and
    // method templates.
    if (cast<CXXMethodDecl>(decl->getUnderlyingDecl()->getAsFunction())
            ->isStatic())
      return false;
  } while (++begin != end);

  return true;
}

UnresolvedMemberExpr::UnresolvedMemberExpr(
    const ASTContext &C, bool HasUnresolvedUsing, Expr *Base, QualType BaseType,
    bool IsArrow, SourceLocation OperatorLoc,
    NestedNameSpecifierLoc QualifierLoc, SourceLocation TemplateKWLoc,
    const DeclarationNameInfo &MemberNameInfo,
    const TemplateArgumentListInfo *TemplateArgs, UnresolvedSetIterator Begin,
    UnresolvedSetIterator End)
    : OverloadExpr(
          UnresolvedMemberExprClass, C, QualifierLoc, TemplateKWLoc,
          MemberNameInfo, TemplateArgs, Begin, End,
          // Dependent
          ((Base && Base->isTypeDependent()) || BaseType->isDependentType()),
          ((Base && Base->isInstantiationDependent()) ||
           BaseType->isInstantiationDependentType()),
          // Contains unexpanded parameter pack
          ((Base && Base->containsUnexpandedParameterPack()) ||
           BaseType->containsUnexpandedParameterPack())),
      IsArrow(IsArrow), HasUnresolvedUsing(HasUnresolvedUsing), Base(Base),
      BaseType(BaseType), OperatorLoc(OperatorLoc) {
  // Check whether all of the members are non-static member functions,
  // and if so, mark give this bound-member type instead of overload type.
  if (hasOnlyNonStaticMemberFunctions(Begin, End))
    setType(C.BoundMemberTy);
}

bool UnresolvedMemberExpr::isImplicitAccess() const {
  if (!Base)
    return true;

  return cast<Expr>(Base)->isImplicitCXXThis();
}

UnresolvedMemberExpr *UnresolvedMemberExpr::Create(
    const ASTContext &C, bool HasUnresolvedUsing, Expr *Base, QualType BaseType,
    bool IsArrow, SourceLocation OperatorLoc,
    NestedNameSpecifierLoc QualifierLoc, SourceLocation TemplateKWLoc,
    const DeclarationNameInfo &MemberNameInfo,
    const TemplateArgumentListInfo *TemplateArgs, UnresolvedSetIterator Begin,
    UnresolvedSetIterator End) {
  bool HasTemplateKWAndArgsInfo = TemplateArgs || TemplateKWLoc.isValid();
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, TemplateArgs ? TemplateArgs->size() : 0);

  void *Mem = C.Allocate(Size, alignof(UnresolvedMemberExpr));
  return new (Mem) UnresolvedMemberExpr(
      C, HasUnresolvedUsing, Base, BaseType, IsArrow, OperatorLoc, QualifierLoc,
      TemplateKWLoc, MemberNameInfo, TemplateArgs, Begin, End);
}

UnresolvedMemberExpr *
UnresolvedMemberExpr::CreateEmpty(const ASTContext &C,
                                  bool HasTemplateKWAndArgsInfo,
                                  unsigned NumTemplateArgs) {
  assert(NumTemplateArgs == 0 || HasTemplateKWAndArgsInfo);
  std::size_t Size =
      totalSizeToAlloc<ASTTemplateKWAndArgsInfo, TemplateArgumentLoc>(
          HasTemplateKWAndArgsInfo, NumTemplateArgs);

  void *Mem = C.Allocate(Size, alignof(UnresolvedMemberExpr));
  auto *E = new (Mem) UnresolvedMemberExpr(EmptyShell());
  E->HasTemplateKWAndArgsInfo = HasTemplateKWAndArgsInfo;
  return E;
}

CXXRecordDecl *UnresolvedMemberExpr::getNamingClass() const {
  // Unlike for UnresolvedLookupExpr, it is very easy to re-derive this.

  // If there was a nested name specifier, it names the naming class.
  // It can't be dependent: after all, we were actually able to do the
  // lookup.
  CXXRecordDecl *Record = nullptr;
  auto *NNS = getQualifier();
  if (NNS && NNS->getKind() != NestedNameSpecifier::Super) {
    const Type *T = getQualifier()->getAsType();
    assert(T && "qualifier in member expression does not name type");
    Record = T->getAsCXXRecordDecl();
    assert(Record && "qualifier in member expression does not name record");
  }
  // Otherwise the naming class must have been the base class.
  else {
    QualType BaseType = getBaseType().getNonReferenceType();
    if (isArrow()) {
      const auto *PT = BaseType->getAs<PointerType>();
      assert(PT && "base of arrow member access is not pointer");
      BaseType = PT->getPointeeType();
    }

    Record = BaseType->getAsCXXRecordDecl();
    assert(Record && "base of member expression does not name record");
  }

  return Record;
}

SizeOfPackExpr *SizeOfPackExpr::Create(ASTContext &Context,
                                       SourceLocation OperatorLoc,
                                       NamedDecl *Pack, SourceLocation PackLoc,
                                       SourceLocation RParenLoc,
                                       Optional<unsigned> Length,
                                       ArrayRef<TemplateArgument> PartialArgs) {
  void *Storage =
      Context.Allocate(totalSizeToAlloc<TemplateArgument>(PartialArgs.size()));
  return new (Storage) SizeOfPackExpr(Context.getSizeType(), OperatorLoc, Pack,
                                      PackLoc, RParenLoc, Length, PartialArgs);
}

SizeOfPackExpr *SizeOfPackExpr::CreateDeserialized(ASTContext &Context,
                                                   unsigned NumPartialArgs) {
  void *Storage =
      Context.Allocate(totalSizeToAlloc<TemplateArgument>(NumPartialArgs));
  return new (Storage) SizeOfPackExpr(EmptyShell(), NumPartialArgs);
}

SubstNonTypeTemplateParmPackExpr::SubstNonTypeTemplateParmPackExpr(
    QualType T, ExprValueKind ValueKind, NonTypeTemplateParmDecl *Param,
    SourceLocation NameLoc, const TemplateArgument &ArgPack)
    : Expr(SubstNonTypeTemplateParmPackExprClass, T, ValueKind, OK_Ordinary,
           true, true, true, true),
      Param(Param), Arguments(ArgPack.pack_begin()),
      NumArguments(ArgPack.pack_size()), NameLoc(NameLoc) {}

TemplateArgument SubstNonTypeTemplateParmPackExpr::getArgumentPack() const {
  return TemplateArgument(llvm::makeArrayRef(Arguments, NumArguments));
}

FunctionParmPackExpr::FunctionParmPackExpr(QualType T, ParmVarDecl *ParamPack,
                                           SourceLocation NameLoc,
                                           unsigned NumParams,
                                           ParmVarDecl *const *Params)
    : Expr(FunctionParmPackExprClass, T, VK_LValue, OK_Ordinary, true, true,
           true, true),
      ParamPack(ParamPack), NameLoc(NameLoc), NumParameters(NumParams) {
  if (Params)
    std::uninitialized_copy(Params, Params + NumParams,
                            getTrailingObjects<ParmVarDecl *>());
}

FunctionParmPackExpr *
FunctionParmPackExpr::Create(const ASTContext &Context, QualType T,
                             ParmVarDecl *ParamPack, SourceLocation NameLoc,
                             ArrayRef<ParmVarDecl *> Params) {
  return new (Context.Allocate(totalSizeToAlloc<ParmVarDecl *>(Params.size())))
      FunctionParmPackExpr(T, ParamPack, NameLoc, Params.size(), Params.data());
}

FunctionParmPackExpr *
FunctionParmPackExpr::CreateEmpty(const ASTContext &Context,
                                  unsigned NumParams) {
  return new (Context.Allocate(totalSizeToAlloc<ParmVarDecl *>(NumParams)))
      FunctionParmPackExpr(QualType(), nullptr, SourceLocation(), 0, nullptr);
}

void MaterializeTemporaryExpr::setExtendingDecl(const ValueDecl *ExtendedBy,
                                                unsigned ManglingNumber) {
  // We only need extra state if we have to remember more than just the Stmt.
  if (!ExtendedBy)
    return;

  // We may need to allocate extra storage for the mangling number and the
  // extended-by ValueDecl.
  if (!State.is<ExtraState *>()) {
    auto *ES = new (ExtendedBy->getASTContext()) ExtraState;
    ES->Temporary = State.get<Stmt *>();
    State = ES;
  }

  auto ES = State.get<ExtraState *>();
  ES->ExtendingDecl = ExtendedBy;
  ES->ManglingNumber = ManglingNumber;
}

TypeTraitExpr::TypeTraitExpr(QualType T, SourceLocation Loc, TypeTrait Kind,
                             ArrayRef<TypeSourceInfo *> Args,
                             SourceLocation RParenLoc, bool Value)
    : Expr(TypeTraitExprClass, T, VK_RValue, OK_Ordinary,
           /*TypeDependent=*/false,
           /*ValueDependent=*/false,
           /*InstantiationDependent=*/false,
           /*ContainsUnexpandedParameterPack=*/false),
      Loc(Loc), RParenLoc(RParenLoc) {
  TypeTraitExprBits.Kind = Kind;
  TypeTraitExprBits.Value = Value;
  TypeTraitExprBits.NumArgs = Args.size();

  auto **ToArgs = getTrailingObjects<TypeSourceInfo *>();

  for (unsigned I = 0, N = Args.size(); I != N; ++I) {
    if (Args[I]->getType()->isDependentType())
      setValueDependent(true);
    if (Args[I]->getType()->isInstantiationDependentType())
      setInstantiationDependent(true);
    if (Args[I]->getType()->containsUnexpandedParameterPack())
      setContainsUnexpandedParameterPack(true);

    ToArgs[I] = Args[I];
  }
}

TypeTraitExpr *TypeTraitExpr::Create(const ASTContext &C, QualType T,
                                     SourceLocation Loc, TypeTrait Kind,
                                     ArrayRef<TypeSourceInfo *> Args,
                                     SourceLocation RParenLoc, bool Value) {
  void *Mem = C.Allocate(totalSizeToAlloc<TypeSourceInfo *>(Args.size()));
  return new (Mem) TypeTraitExpr(T, Loc, Kind, Args, RParenLoc, Value);
}

TypeTraitExpr *TypeTraitExpr::CreateDeserialized(const ASTContext &C,
                                                 unsigned NumArgs) {
  void *Mem = C.Allocate(totalSizeToAlloc<TypeSourceInfo *>(NumArgs));
  return new (Mem) TypeTraitExpr(EmptyShell());
}

CUDAKernelCallExpr::CUDAKernelCallExpr(Expr *Fn, CallExpr *Config,
                                       ArrayRef<Expr *> Args, QualType Ty,
                                       ExprValueKind VK, SourceLocation RP,
                                       unsigned MinNumArgs)
    : CallExpr(CUDAKernelCallExprClass, Fn, /*PreArgs=*/Config, Args, Ty, VK,
               RP, MinNumArgs, NotADL) {}

CUDAKernelCallExpr::CUDAKernelCallExpr(unsigned NumArgs, EmptyShell Empty)
    : CallExpr(CUDAKernelCallExprClass, /*NumPreArgs=*/END_PREARG, NumArgs,
               Empty) {}

CUDAKernelCallExpr *
CUDAKernelCallExpr::Create(const ASTContext &Ctx, Expr *Fn, CallExpr *Config,
                           ArrayRef<Expr *> Args, QualType Ty, ExprValueKind VK,
                           SourceLocation RP, unsigned MinNumArgs) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned NumArgs = std::max<unsigned>(Args.size(), MinNumArgs);
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/END_PREARG, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CUDAKernelCallExpr) + SizeOfTrailingObjects,
                           alignof(CUDAKernelCallExpr));
  return new (Mem) CUDAKernelCallExpr(Fn, Config, Args, Ty, VK, RP, MinNumArgs);
}

CUDAKernelCallExpr *CUDAKernelCallExpr::CreateEmpty(const ASTContext &Ctx,
                                                    unsigned NumArgs,
                                                    EmptyShell Empty) {
  // Allocate storage for the trailing objects of CallExpr.
  unsigned SizeOfTrailingObjects =
      CallExpr::sizeOfTrailingObjects(/*NumPreArgs=*/END_PREARG, NumArgs);
  void *Mem = Ctx.Allocate(sizeof(CUDAKernelCallExpr) + SizeOfTrailingObjects,
                           alignof(CUDAKernelCallExpr));
  return new (Mem) CUDAKernelCallExpr(NumArgs, Empty);
}
//
// ReflexprExpr
ReflexprExpr::ReflexprExpr(EmptyShell Empty) : Expr(ReflexprExprClass, Empty) {}

// reflexpr([::])
ReflexprExpr::ReflexprExpr(QualType resultType, MetaobjectKind kind,
                           SourceLocation opLoc, SourceLocation endLoc)
    : Expr(ReflexprExprClass, resultType, VK_RValue, OK_Ordinary,
           false,  // not type dependent
           false,  // not value dependent
           false,  // not instantiation dependent
           false), // no unexpanded parameter pack
      OpLoc(opLoc), EndLoc(endLoc) {

  setKind(kind);
  setSeqKind(MOSK_None);
  setArgKind(REAK_Nothing);
  Argument.Nothing = nullptr;
  setRemoveSugar(false);
  setHideProtected(false);
  setHidePrivate(false);
}

ReflexprExpr::ReflexprExpr(QualType resultType, tok::TokenKind specTok,
                           SourceLocation opLoc, SourceLocation endLoc)
    : Expr(ReflexprExprClass, resultType, VK_RValue, OK_Ordinary,
           false,  // not type dependent
           false,  // not value dependent
           false,  // not instantiation dependent
           false), // no unexpanded parameter pack
      OpLoc(opLoc), EndLoc(endLoc) {

  setKind(MOK_Specifier);
  setSeqKind(MOSK_None);
  setArgKind(REAK_Specifier);
  Argument.SpecTok = specTok;
  setRemoveSugar(false);
  setHideProtected(false);
  setHidePrivate(false);
}

ReflexprExpr::ReflexprExpr(QualType resultType, const NamedDecl *nDecl,
                           SourceLocation opLoc, SourceLocation endLoc)
    : Expr(ReflexprExprClass, resultType, VK_RValue, OK_Ordinary,
           false, // TODO[reflexpr]
           false, false,
           false), // no unexpanded parameter pack
      OpLoc(opLoc), EndLoc(endLoc) {

  if (isa<NamespaceAliasDecl>(nDecl)) {
    setKind(MOK_NamespaceAlias);
  } else if (isa<NamespaceDecl>(nDecl)) {
    setKind(MOK_Namespace);
  } else if (isa<EnumDecl>(nDecl)) {
    if (nDecl->isCXXClassMember()) {
      setKind(MOK_MemberEnum);
    } else {
      setKind(MOK_Enum);
    }
  } else if (const auto *RD = dyn_cast<CXXRecordDecl>(nDecl)) {
    if (nDecl->isCXXClassMember()) {
      if (RD && RD->isUnion()) {
        setKind(MOK_MemberRecord);
      } else {
        setKind(MOK_MemberClass);
      }
    } else {
      if (RD && RD->isUnion()) {
        setKind(MOK_Record);
      } else {
        setKind(MOK_Class);
      }
    }
  } else if (isa<RecordDecl>(nDecl)) {
    if (nDecl->isCXXClassMember()) {
      setKind(MOK_MemberRecord);
    } else {
      setKind(MOK_Record);
    }
  } else if (const auto *TND = dyn_cast<TypedefNameDecl>(nDecl)) {
    const Type *UT = TND->getUnderlyingType()->getUnqualifiedDesugaredType();
    const TagDecl *TD = nullptr;
    if (const auto *UTT = dyn_cast<TagType>(UT)) {
      TD = UTT->getDecl();
    }
    if (nDecl->isCXXClassMember()) {
      if (TD && TD->isUnion()) {
        setKind(MOK_MemberRecordAlias);
      } else if (TD && (TD->isClass() || TD->isStruct())) {
        setKind(MOK_MemberClassAlias);
      } else if (TD && TD->isEnum()) {
        setKind(MOK_MemberEnumAlias);
      } else {
        if (TD && isa<ClassTemplateSpecializationDecl>(TD)) {
          setKind(MOK_MemberClassAlias);
        } else {
          setKind(MOK_MemberTypeAlias);
        }
      }
    } else {
      if (TD && TD->isUnion()) {
        setKind(MOK_RecordAlias);
      } else if (TD && (TD->isClass() || TD->isStruct())) {
        setKind(MOK_ClassAlias);
      } else if (TD && TD->isEnum()) {
        setKind(MOK_EnumAlias);
      } else {
        if (TD && isa<ClassTemplateSpecializationDecl>(TD)) {
          setKind(MOK_ClassAlias);
        } else {
          setKind(MOK_TypeAlias);
        }
      }
    }
  } else if (isa<TemplateTypeParmDecl>(nDecl)) {
    setKind(MOK_TplTypeParam);
  } else if (isa<TypeDecl>(nDecl)) {
    setKind(MOK_Type);
  } else if (isa<FieldDecl>(nDecl)) {
    setKind(MOK_DataMember);
  } else if (const auto *VD = dyn_cast<VarDecl>(nDecl)) {
    if (VD->isStaticDataMember()) {
      setKind(MOK_DataMember);
    } else {
      setKind(MOK_Variable);
    }
  } else if (isa<EnumConstantDecl>(nDecl)) {
    setKind(MOK_Enumerator);
  } else if (isa<CXXMethodDecl>(nDecl)) {
    setKind(MOK_MemberFunction);
  } else if (isa<FunctionDecl>(nDecl)) {
    setKind(MOK_NamedFunction);
  } else {
    setKind(MOK_Unknown);
  }
  setSeqKind(MOSK_None);
  setArgKind(REAK_NamedDecl);
  Argument.ReflDecl = nDecl;
  setRemoveSugar(false);
  setHideProtected(false);
  setHidePrivate(false);
}

ReflexprExpr::ReflexprExpr(QualType resultType, const TypeSourceInfo *TInfo,
                           bool removeSugar, SourceLocation opLoc,
                           SourceLocation endLoc)
    : Expr(ReflexprExprClass, resultType, VK_RValue, OK_Ordinary, false,
           TInfo->getType()->isDependentType(),
           TInfo->getType()->isInstantiationDependentType(),
           TInfo->getType()->containsUnexpandedParameterPack()),
      OpLoc(opLoc), EndLoc(endLoc) {

  const Type *RT = TInfo->getType().getTypePtr();

  // TODO[reflexpr] this is here just for devel/debugging can be removed later
  Type::TypeClass tc = RT->getTypeClass();
  (void)tc;

  bool isAlias = false;
  if (const auto *STTPT = dyn_cast<SubstTemplateTypeParmType>(RT)) {
    isAlias = true;
    RT = STTPT->getReplacementType().getTypePtr();
  } else if (isa<TypedefType>(RT)) {
    isAlias = true;
  }
  isAlias &= !removeSugar;

  RT = RT->getUnqualifiedDesugaredType();

  if (isa<TemplateTypeParmType>(RT)) {
    setKind(MOK_TplTypeParam);
  } else if (isa<RecordType>(RT)) {
    setKind(isAlias ? MOK_ClassAlias : MOK_Class);
  } else if (isa<EnumType>(RT)) {
    setKind(isAlias ? MOK_EnumAlias : MOK_Enum);
  } else {
    setKind(isAlias ? MOK_TypeAlias : MOK_Type);
  }
  setSeqKind(MOSK_None);
  setArgKind(REAK_TypeInfo);
  Argument.TypeInfo = TInfo;
  setRemoveSugar(removeSugar);
  setHideProtected(false);
  setHidePrivate(false);
}

ReflexprExpr::ReflexprExpr(QualType resultType,
                           const CXXBaseSpecifier *baseSpec,
                           SourceLocation opLoc, SourceLocation endLoc)
    : Expr(ReflexprExprClass, resultType, VK_RValue, OK_Ordinary,
           false,  // not type dependent
           false,  // not value dependent TODO[reflexpr]:are these always right?
           false,  // not instantiation dependent
           false), // no unexpanded parameter pack
      OpLoc(opLoc), EndLoc(endLoc) {

  setKind(MOK_Inheritance);
  setSeqKind(MOSK_None);
  setArgKind(REAK_BaseSpecifier);
  Argument.BaseSpec = baseSpec;
  setRemoveSugar(false);
  setHideProtected(false);
  setHidePrivate(false);
}

ReflexprExpr::ReflexprExpr(const ReflexprExpr &that)
    : Expr(ReflexprExprClass, that.getType(), VK_RValue, OK_Ordinary,
           that.isTypeDependent(), that.isValueDependent(),
           that.isInstantiationDependent(),
           that.containsUnexpandedParameterPack()),
      OpLoc(that.getOperatorLoc()), EndLoc(that.getEndLoc()) {

  setKind(that.getKind());
  setSeqKind(that.getSeqKind());
  setArgKind(that.getArgKind());
  switch (getArgKind()) {
  case REAK_Nothing:
    Argument.Nothing = that.Argument.Nothing;
    break;
  case REAK_Specifier:
    Argument.SpecTok = that.Argument.SpecTok;
    break;
  case REAK_NamedDecl:
    Argument.ReflDecl = that.Argument.ReflDecl;
    break;
  case REAK_TypeInfo:
    Argument.TypeInfo = that.Argument.TypeInfo;
    break;
  case REAK_BaseSpecifier:
    Argument.BaseSpec = that.Argument.BaseSpec;
    break;
  }
  setRemoveSugar(that.getRemoveSugar());
  setHideProtected(that.getHideProtected());
  setHidePrivate(that.getHidePrivate());
}

ReflexprExpr *ReflexprExpr::getGSReflexprExpr(ASTContext &Context,
                                              SourceLocation opLoc,
                                              SourceLocation endLoc) {
  if (ReflexprExpr *E = Context.findGlobalScopeReflexpr())
    return E;
  return Context.cacheGlobalScopeReflexpr(new (Context) ReflexprExpr(
      Context.getMetaobjectIdType(), MOK_GlobalScope, opLoc, endLoc));
}

ReflexprExpr *ReflexprExpr::getNoSpecifierReflexprExpr(ASTContext &Context,
                                                       SourceLocation opLoc,
                                                       SourceLocation endLoc) {
  if (ReflexprExpr *E = Context.findNoSpecifierReflexpr())
    return E;
  return Context.cacheNoSpecifierReflexpr(new (Context) ReflexprExpr(
      Context.getMetaobjectIdType(), MOK_Specifier, opLoc, endLoc));
}

ReflexprExpr *ReflexprExpr::getSpecifierReflexprExpr(ASTContext &Context,
                                                     tok::TokenKind specTok,
                                                     SourceLocation opLoc,
                                                     SourceLocation endLoc) {
  if (ReflexprExpr *E = Context.findSpecifierReflexpr(specTok))
    return E;
  return Context.cacheSpecifierReflexpr(
      specTok, new (Context) ReflexprExpr(Context.getMetaobjectIdType(),
                                          specTok, opLoc, endLoc));
}

ReflexprExpr *ReflexprExpr::getNamedDeclReflexprExpr(ASTContext &Context,
                                                     const NamedDecl *nDecl,
                                                     SourceLocation opLoc,
                                                     SourceLocation endLoc) {
  if (ReflexprExpr *E = Context.findNamedDeclReflexpr(nDecl))
    return E;
  return Context.cacheNamedDeclReflexpr(
      nDecl, new (Context) ReflexprExpr(Context.getMetaobjectIdType(), nDecl,
                                        opLoc, endLoc));
}

ReflexprExpr *ReflexprExpr::getTypeReflexprExpr(ASTContext &Context,
                                                const TypeSourceInfo *TInfo,
                                                bool removeSugar,
                                                SourceLocation opLoc,
                                                SourceLocation endLoc) {
  // TODO[reflexpr] cache in ASTContext when possible
  return new (Context) ReflexprExpr(Context.getMetaobjectIdType(), TInfo,
                                    removeSugar, opLoc, endLoc);
}

ReflexprExpr *ReflexprExpr::getTypeReflexprExpr(ASTContext &Context,
                                                QualType Ty, bool removeSugar,
                                                SourceLocation opLoc,
                                                SourceLocation endLoc) {
  // TODO[reflexpr] cache in ASTContext when possible
  return new (Context) ReflexprExpr(Context.getMetaobjectIdType(),
                                    Context.getTrivialTypeSourceInfo(Ty),
                                    removeSugar, opLoc, endLoc);
}

ReflexprExpr *ReflexprExpr::getBaseSpecifierReflexprExpr(
    ASTContext &Context, const CXXBaseSpecifier *baseSpec, SourceLocation opLoc,
    SourceLocation endLoc) {
  // TODO[reflexpr] cache in ASTContext when possible
  return new (Context)
      ReflexprExpr(Context.getMetaobjectIdType(), baseSpec, opLoc, endLoc);
}

ReflexprExpr *ReflexprExpr::getSeqReflexprExpr(ASTContext &Context,
                                               ReflexprExpr *that,
                                               MetaobjectSequenceKind MoSK) {
  assert(that != nullptr);
  // TODO[reflexpr] cache in ASTContext when possible
  ReflexprExpr *Res = new (Context) ReflexprExpr(*that);
  Res->setKind(MOK_ObjectSequence);
  Res->setSeqKind(MoSK);
  return Res;
}

ReflexprExpr *ReflexprExpr::getHideProtectedReflexprExpr(ASTContext &Context,
                                                         ReflexprExpr *that) {
  assert(that != nullptr);
  // TODO[reflexpr] cache in ASTContext when possible
  ReflexprExpr *Res = new (Context) ReflexprExpr(*that);
  Res->setHideProtected(true);
  Res->setHidePrivate(true);
  return Res;
}

ReflexprExpr *ReflexprExpr::getHidePrivateReflexprExpr(ASTContext &Context,
                                                       ReflexprExpr *that) {
  assert(that != nullptr);
  // TODO[reflexpr] cache in ASTContext when possible
  ReflexprExpr *Res = new (Context) ReflexprExpr(*that);
  Res->setHidePrivate(true);
  return Res;
}

ReflexprExpr *ReflexprExpr::fromMetaobjectId(ASTContext &Context,
                                             uintptr_t moid) {
  return reinterpret_cast<ReflexprExpr *>(Context.decodeMetaobjectId(moid));
}

uintptr_t ReflexprExpr::toMetaobjectId(ASTContext &Context,
                                       const ReflexprExpr *that) {
  return Context.encodeMetaobjectId(reinterpret_cast<uintptr_t>(that));
}

ReflexprExpr *ReflexprExpr::fromExpr(ASTContext &Context, Expr *E) {
  if (ReflexprExpr *RE = dyn_cast<ReflexprExpr>(E)) {
    return RE;
  }

  if (MetaobjectIdExpr *MIE = dyn_cast<MetaobjectIdExpr>(E)) {
    return MIE->asReflexprExpr(Context);
  }

  llvm::APSInt apsi;
  if (E->isMetaobjectIdExpr(apsi, nullptr, Context)) {
    return fromMetaobjectId(Context, apsi.getZExtValue());
  }

  return nullptr;
}

StringRef ReflexprExpr::getMetaobjectKindName(MetaobjectKind MoK) {
  switch (MoK) {
  case MOK_Specifier:
    return "a specifier";
  case MOK_Inheritance:
    return "an inheritance";
  case MOK_GlobalScope:
    return "the global scope";
  case MOK_Namespace:
    return "a namespace";
  case MOK_NamespaceAlias:
    return "a namespace alias";
  case MOK_Type:
    return "a type";
  case MOK_Enum:
    return "an enum";
  case MOK_Record:
    return "a record";
  case MOK_Class:
    return "a class";
  case MOK_NamedFunction:
    return "a function";
  case MOK_TypeAlias:
    return "a type alias";
  case MOK_EnumAlias:
    return "a enum alias";
  case MOK_RecordAlias:
    return "a record alias";
  case MOK_ClassAlias:
    return "a class alias";
  case MOK_TplTypeParam:
    return "a template type parameter";
  case MOK_Variable:
    return "a variable";
  case MOK_DataMember:
    return "a data member";
  case MOK_MemberType:
    return "a member type";
  case MOK_MemberTypeAlias:
    return "a member type alias";
  case MOK_MemberRecord:
    return "a member record";
  case MOK_MemberRecordAlias:
    return "a member record alias";
  case MOK_MemberClass:
    return "a member class";
  case MOK_MemberClassAlias:
    return "a member class alias";
  case MOK_MemberEnum:
    return "a member enum";
  case MOK_MemberEnumAlias:
    return "a member enum alias";
  case MOK_MemberFunction:
    return "a member function";
  case MOK_Enumerator:
    return "a enumerator";
  case MOK_ObjectSequence:
    return "a metaobject sequence";
  case MOK_Unknown:
    break;
  }
  return StringRef();
}

static MetaobjectConcept
translateMetaobjectKindToMetaobjectConcept(MetaobjectKind MoK) {
  switch (MoK) {
  case MOK_Specifier:
    return MOC_Specifier;
  case MOK_Inheritance:
    return MOC_Inheritance;
  case MOK_GlobalScope:
    return MOC_GlobalScope;
  case MOK_Namespace:
    return MOC_Namespace;
  case MOK_NamespaceAlias:
    return MOC_NamespaceAlias;
  case MOK_Type:
    return MOC_Type;
  case MOK_Enum:
    return MOC_Enum;
  case MOK_Record:
    return MOC_Record;
  case MOK_Class:
    return MOC_Class;
  case MOK_TypeAlias:
    return MOC_TypeAlias;
  case MOK_EnumAlias:
    return MOC_EnumAlias;
  case MOK_RecordAlias:
    return MOC_RecordAlias;
  case MOK_ClassAlias:
    return MOC_ClassAlias;
  case MOK_TplTypeParam:
    return MOC_TplTypeParam;
  case MOK_Variable:
    return MOC_Variable;
  case MOK_NamedFunction:
    return MOC_NamedFunction;
  case MOK_DataMember:
    return MOC_DataMember;
  case MOK_MemberType:
    return MOC_MemberType;
  case MOK_MemberTypeAlias:
    return MOC_MemberTypeAlias;
  case MOK_MemberRecord:
    return MOC_MemberRecord;
  case MOK_MemberRecordAlias:
    return MOC_MemberRecordAlias;
  case MOK_MemberClass:
    return MOC_MemberClass;
  case MOK_MemberClassAlias:
    return MOC_MemberClassAlias;
  case MOK_MemberEnum:
    return MOC_MemberEnum;
  case MOK_MemberEnumAlias:
    return MOC_MemberEnumAlias;
  case MOK_MemberFunction:
    return MOC_MemberFunction;
  case MOK_Enumerator:
    return MOC_Enumerator;
  case MOK_ObjectSequence:
    return MOC_ObjectSequence;
  case MOK_Unknown:
    llvm_unreachable("Metaobject kind must be known at this point!");
  }
  llvm_unreachable("Metaobject kind not implemented!");
}

MetaobjectConcept ReflexprExpr::getCategory(void) const {
  return translateMetaobjectKindToMetaobjectConcept(getKind());
}

const NamedDecl *ReflexprExpr::findTypeDecl(QualType Ty) {
  if (const auto *TdT = dyn_cast<TypedefType>(Ty)) {
    return TdT->getDecl();
  } else if (const auto *TgT = dyn_cast<TagType>(Ty)) {
    return TgT->getDecl();
  } else if (const auto *TST = dyn_cast<TemplateSpecializationType>(Ty)) {
    return TST->getTemplateName().getAsTemplateDecl();
  } else if (const auto *STTPT = dyn_cast<SubstTemplateTypeParmType>(Ty)) {
    return STTPT->getReplacedParameter()->getDecl();
  } else if (const auto *TTPT = dyn_cast<TemplateTypeParmType>(Ty)) {
    return TTPT->getDecl();
  }
  return nullptr;
}

QualType ReflexprExpr::getBaseArgumentType(ASTContext &,
                                           bool removeSugar) const {

  QualType Res = getArgumentType();
  removeSugar |= isa<DecltypeType>(Res);

  // TODO[reflexpr] this is here just for devel/debugging can be removed later
  Type::TypeClass tc = Res->getTypeClass();
  (void)tc;

  if (removeSugar)
    Res = Res.getCanonicalType();

  if (const auto *ET = dyn_cast<ElaboratedType>(Res)) {
    Res = ET->getNamedType();
  }

  while (true) {
    if (const auto *PT = dyn_cast<PointerType>(Res)) {
      Res = PT->getPointeeType();
    } else if (const auto *RT = dyn_cast<ReferenceType>(Res)) {
      Res = RT->getPointeeType();
    } else if (const auto *AT = dyn_cast<ArrayType>(Res)) {
      Res = AT->getElementType();
    } else {
      break;
    }
  }

  return Res;
}

const NamedDecl *ReflexprExpr::findArgumentNamedDecl(ASTContext &Ctx,
                                                     bool removeSugar) const {
  if (isArgumentNamedDecl())
    return getArgumentNamedDecl();
  if (isArgumentType())
    return findTypeDecl(getBaseArgumentType(Ctx, removeSugar));
  return nullptr;
}

const ValueDecl *ReflexprExpr::findArgumentValueDecl(ASTContext &Ctx) const {
  return dyn_cast<ValueDecl>(findArgumentNamedDecl(Ctx, false));
}

bool ReflexprExpr::reflectsType(void) const {
  if (isArgumentType())
    return true;

  if (isArgumentNamedDecl())
    return isa<TypeDecl>(getArgumentNamedDecl());

  return false;
}

QualType ReflexprExpr::getReflectedType(void) const {
  if (isArgumentType())
    return getArgumentType();

  if (isArgumentNamedDecl()) {
    if (const auto *TDND = dyn_cast<TypedefNameDecl>(getArgumentNamedDecl())) {
      return TDND->getUnderlyingType();
    } else if (const auto *TD = dyn_cast<TypeDecl>(getArgumentNamedDecl())) {
      return QualType(TD->getTypeForDecl(), 0);
    }
  }

  return QualType();
}

bool ReflexprExpr::isArgumentDependent(void) const {
  if (isArgumentNamedDecl()) {
    const NamedDecl *ND = getArgumentNamedDecl();
    return isa<TemplateTypeParmDecl>(ND);
  }
  return false;
}

AccessSpecifier ReflexprExpr::getArgumentAccess(ASTContext &Ctx) const {
  if (isArgumentBaseSpecifier()) {
    return getArgumentBaseSpecifier()->getAccessSpecifier();
  }

  if (const NamedDecl *ND = findArgumentNamedDecl(Ctx, true)) {
    return ND->getAccess();
  }

  return AS_none;
}

Stmt::child_range ReflexprExpr::children() {
  // TODO[reflexpr]
  return child_range(child_iterator(), child_iterator());
}

// MetaobjectOpExpr
bool MetaobjectOpExprBase::_anyTypeDependent(unsigned arity, Expr **argExpr) {
  for (unsigned i = 0; i < arity; ++i) {
    if (argExpr[i]) {
      if (argExpr[i]->isTypeDependent())
        return true;
    }
  }
  return false;
}

bool MetaobjectOpExprBase::_anyValueDependent(unsigned arity, Expr **argExpr) {
  for (unsigned i = 0; i < arity; ++i) {
    if (argExpr[i]) {
      if (argExpr[i]->isValueDependent())
        return true;
    }
  }
  return false;
}

bool MetaobjectOpExprBase::_anyInstDependent(unsigned arity, Expr **argExpr) {
  for (unsigned i = 0; i < arity; ++i) {
    if (argExpr[i]) {
      if (argExpr[i]->isInstantiationDependent())
        return true;
    }
  }
  return false;
}

bool MetaobjectOpExprBase::_anyHasUnexpandedPack(unsigned arity,
                                                 Expr **argExpr) {
  for (unsigned i = 0; i < arity; ++i) {
    if (argExpr[i]) {
      if (argExpr[i]->containsUnexpandedParameterPack())
        return true;
    }
  }
  return false;
}

QualType MetaobjectOpExprBase::getResultKindType(ASTContext &Ctx,
                                                 unsigned arity, Expr **argExpr,
                                                 MetaobjectOpResult OpRes) {
  switch (OpRes) {
  case MOOR_Metaobj:
    return Ctx.MetaobjectIdTy;
  case MOOR_ULong:
    return Ctx.UnsignedLongTy;
  case MOOR_UInt:
    return Ctx.UnsignedIntTy;
  case MOOR_Bool:
    return Ctx.BoolTy;
  case MOOR_Const:
    break;
  case MOOR_Pointer:
    llvm_unreachable("Pointer-returning operations are handled elsewhere");
  case MOOR_String:
    llvm_unreachable("String-returning operations are handled elsewhere");
  }

  assert(OpRes == MOOR_Const);

  if (argExpr[0]->isInstantiationDependent()) {
    return Ctx.DependentTy;
  }

  ReflexprExpr *RE = ReflexprExpr::fromExpr(Ctx, argExpr[0]);

  if (RE->isArgumentNamedDecl()) {
    if (const auto *ND = RE->getArgumentNamedDecl()) {
      if (const auto *VD = dyn_cast<ValueDecl>(ND)) {
        return VD->getType();
      }
    }
  }
  llvm_unreachable("Unable to find the type of constant-returning operation");
}

AccessSpecifier MetaobjectOpExprBase::getArgumentAccess(ASTContext &Ctx,
                                                        uintptr_t moid) {
  return asReflexpr(Ctx, moid)->getArgumentAccess(Ctx);
}

bool MetaobjectOpExprBase::queryExprUIntValue(ASTContext &Ctx, uint64_t &value,
                                              Expr *E) {
  llvm::APSInt apsi;
  if (E->isIntegerConstantExpr(apsi, Ctx)) {
    value = apsi.getZExtValue();
    return true;
  }
  return false;
}

bool MetaobjectOpExprBase::queryExprMetaobjectId(ASTContext &Ctx,
                                                 uintptr_t &moid, void *EvlInfo,
                                                 Expr *E) {
  llvm::APSInt apsi;
  if (E->isMetaobjectIdExpr(apsi, EvlInfo, Ctx)) {
    moid = apsi.getZExtValue();
    return true;
  }

  if (MetaobjectIdExpr *moie = dyn_cast<MetaobjectIdExpr>(E)) {
    moid = moie->getValue();
    return true;
  }

  if (ReflexprExpr *ree = dyn_cast<ReflexprExpr>(E)) {
    moid = ree->getIdValue(Ctx);
    return true;
  }
  return false;
}

llvm::APSInt MetaobjectOpExprBase::makeBoolResult(ASTContext &, bool v) {
  // TODO[reflexpr] is there a better way to get true/false APSInt?
  return v ? llvm::APSInt::getMaxValue(1, true)
           : llvm::APSInt::getMinValue(1, true);
}

llvm::APSInt MetaobjectOpExprBase::makeUIntResult(ASTContext &Ctx, unsigned v) {
  unsigned w = Ctx.getTargetInfo().getIntWidth();
  return llvm::APSInt(llvm::APInt(w, v));
}

llvm::APSInt MetaobjectOpExprBase::makeULongResult(ASTContext &Ctx,
                                                   uint64_t v) {
  unsigned w = Ctx.getTargetInfo().getLongWidth();
  return llvm::APSInt(llvm::APInt(w, v));
}

llvm::APSInt MetaobjectOpExprBase::makeMetaobjResult(ASTContext &Ctx,
                                                     ReflexprExpr *RE) {
  unsigned w = 8 * sizeof(void *);
  return llvm::APSInt(llvm::APInt(w, ReflexprExpr::toMetaobjectId(Ctx, RE)));
}

llvm::APSInt MetaobjectOpExprBase::makeConstResult(ASTContext &,
                                                   llvm::APSInt R) {
  return R;
}

llvm::APSInt MetaobjectOpExprBase::opGetConstant(ASTContext &Ctx,
                                                 uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentNamedDecl()) {
    if (const auto *ND = argRE->getArgumentNamedDecl()) {
      if (const auto *ECD = dyn_cast<EnumConstantDecl>(ND)) {
        return ECD->getInitVal();
      }
    }
  }
  llvm_unreachable("Unable to get constant value!");
}

// __metaobj_{operation}
UnaryMetaobjectOpExpr::UnaryMetaobjectOpExpr(ASTContext &Ctx,
                                             UnaryMetaobjectOp Oper,
                                             MetaobjectOpResult OpRes,
                                             QualType resultType, Expr *argExpr,
                                             SourceLocation opLoc,
                                             SourceLocation endLoc)
    : Expr(UnaryMetaobjectOpExprClass, resultType, VK_RValue, OK_Ordinary,
           _anyTypeDependent(1, &argExpr), _anyValueDependent(1, &argExpr),
           _anyInstDependent(1, &argExpr), _anyHasUnexpandedPack(1, &argExpr)),
      ArgExpr(argExpr), OpLoc(opLoc), EndLoc(endLoc) {

  setKind(Oper);
  setResultKind(OpRes);
}

StringRef UnaryMetaobjectOpExpr::getOperationSpelling(UnaryMetaobjectOp MoOp) {
  switch (MoOp) {
#define METAOBJECT_OP_1(Spelling, R, Name, K)                                  \
  case UMOO_##Name:                                                            \
    return #Spelling;
#include "clang/Basic/TokenKinds.def"
  }
  return StringRef();
}

bool UnaryMetaobjectOpExpr::isOperationApplicable(MetaobjectKind MoK,
                                                  UnaryMetaobjectOp MoOp) {

  MetaobjectConcept MoC = translateMetaobjectKindToMetaobjectConcept(MoK);
  switch (MoOp) {
  case UMOO_GetIdValue:
#define METAOBJECT_TRAIT(S, Concept, K) case UMOO_IsMeta##Concept:
#include "clang/Basic/TokenKinds.def"
  case UMOO_SourceFileLen:
  case UMOO_GetSourceFile:
  case UMOO_GetSourceLine:
  case UMOO_GetSourceColumn:
    return true;
  case UMOO_IsAnonymous:
  case UMOO_BaseNameLen:
  case UMOO_GetBaseName:
  case UMOO_DisplayNameLen:
  case UMOO_GetDisplayName:
    return conceptIsA(MoC, MOC_Named);
  case UMOO_IsScopedEnum:
    return conceptIsA(MoC, MOC_Enum);
  case UMOO_GetScope:
    return conceptIsA(MoC, MOC_ScopeMember);
  case UMOO_GetType:
    return conceptIsA(MoC, MOC_Typed);
  case UMOO_GetAliased:
    return conceptIsA(MoC, MOC_Alias);
  case UMOO_GetTagSpecifier:
  case UMOO_IsEnum:
  case UMOO_IsClass:
  case UMOO_IsStruct:
  case UMOO_IsUnion:
    return conceptIsA(MoC, MOC_TagType);
  case UMOO_GetBaseClasses:
    return conceptIsA(MoC, MOC_Class);
  case UMOO_GetMemberTypes:
  case UMOO_GetMemberVariables:
  case UMOO_GetMemberConstants:
    return conceptIsA(MoC, MOC_Scope);
  case UMOO_GetBaseClass:
    return conceptIsA(MoC, MOC_Inheritance);
  case UMOO_GetAccessSpecifier:
  case UMOO_IsPublic:
  case UMOO_IsProtected:
  case UMOO_IsPrivate:
    return conceptIsA(MoC, MOC_RecordMember) ||
           conceptIsA(MoC, MOC_Inheritance);
  case UMOO_IsStatic:
    return conceptIsA(MoC, MOC_Variable);
  case UMOO_IsVirtual:
    return conceptIsA(MoC, MOC_Inheritance);
  case UMOO_GetPointer:
    return conceptIsA(MoC, MOC_Variable);
  case UMOO_GetConstant:
    return conceptIsA(MoC, MOC_Constant);
  case UMOO_HideProtected:
  case UMOO_HidePrivate:
  case UMOO_GetSize:
    return conceptIsA(MoC, MOC_ObjectSequence);
  }
  return false;
}

bool UnaryMetaobjectOpExpr::getTraitValue(UnaryMetaobjectOp MoOp,
                                          MetaobjectConcept Cat) {
  switch (MoOp) {
#define METAOBJECT_TRAIT(S, Concept, K)                                        \
  case UMOO_IsMeta##Concept:                                                   \
    return conceptIsA(Cat, MOC_##Concept);
#include "clang/Basic/TokenKinds.def"
  default:
    llvm_unreachable("Not a metaobject trait operation");
  }
}

uintptr_t UnaryMetaobjectOpExpr::opGetIdValue(ASTContext &, uintptr_t moid) {
  return moid;
}

std::size_t UnaryMetaobjectOpExpr::opSourceFileLen(ASTContext &Ctx,
                                                   uintptr_t moid) {
  return opGetSourceFile(Ctx, moid).size();
}

std::string UnaryMetaobjectOpExpr::opGetSourceFile(ASTContext &Ctx,
                                                   uintptr_t moid) {
  StringRef result;
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    SourceLocation L = ND->getLocation();
    result = Ctx.getSourceManager().getFilename(L);
  }
  return result;
}

unsigned UnaryMetaobjectOpExpr::opGetSourceLine(ASTContext &Ctx,
                                                uintptr_t moid) {
  unsigned result = 0;
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    SourceLocation L = ND->getLocation();
    result = Ctx.getSourceManager().getSpellingLineNumber(L);
  }
  return result;
}

unsigned UnaryMetaobjectOpExpr::opGetSourceColumn(ASTContext &Ctx,
                                                  uintptr_t moid) {
  unsigned result = 0;
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    SourceLocation L = ND->getLocation();
    result = Ctx.getSourceManager().getSpellingColumnNumber(L);
  }
  return result;
}

bool UnaryMetaobjectOpExpr::opIsAnonymous(ASTContext &Ctx, uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentSpecifier()) {
    return false;
  } else if (argRE->isArgumentNamedDecl()) {
    return argRE->getArgumentNamedDecl()->getName().empty();
  } else if (argRE->isArgumentType()) {
    QualType RT = argRE->getBaseArgumentType(Ctx);

    if (const NamedDecl *ND = ReflexprExpr::findTypeDecl(RT)) {
      return ND->getName().empty();
    } else if (isa<BuiltinType>(RT)) {
      return false;
    } else if (RT.getBaseTypeIdentifier()) {
      return RT.getBaseTypeIdentifier()->getName().empty();
    }
  }
  return true;
}

std::size_t UnaryMetaobjectOpExpr::opBaseNameLen(ASTContext &Ctx,
                                                 uintptr_t moid) {
  return opGetBaseName(Ctx, moid).size();
}

std::string UnaryMetaobjectOpExpr::opGetBaseName(ASTContext &Ctx,
                                                 uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentGlobalScope()) {
    return StringRef();
  } else if (argRE->isArgumentNoSpecifier()) {
    return StringRef();
  } else if (argRE->isArgumentSpecifier()) {
    return StringRef(tok::getKeywordSpelling(
        asReflexpr(Ctx, moid)->getArgumentSpecifierKind()));
  } else if (argRE->isArgumentNamedDecl()) {
    return argRE->getArgumentNamedDecl()->getName();
  } else if (argRE->isArgumentType()) {
    QualType RT = argRE->getBaseArgumentType(Ctx);

    if (!RT.isNull()) {
      if (const NamedDecl *ND = ReflexprExpr::findTypeDecl(RT)) {
        return ND->getName();
      } else if (const auto *BT = dyn_cast<BuiltinType>(RT)) {
        return BT->getName(Ctx.getPrintingPolicy());
      } else if (RT.getBaseTypeIdentifier()) {
        return RT.getBaseTypeIdentifier()->getName();
      } else
        return std::string();
    }
  }
  llvm_unreachable("Unable to get metaobject name!");
}

std::size_t UnaryMetaobjectOpExpr::opDisplayNameLen(ASTContext &Ctx,
                                                    uintptr_t moid) {
  return opGetDisplayName(Ctx, moid).size();
}

std::string UnaryMetaobjectOpExpr::opGetDisplayName(ASTContext &Ctx,
                                                    uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentGlobalScope()) {
    return std::string("::", 2);
  } else if (argRE->isArgumentNamedDecl()) {
    return argRE->getArgumentNamedDecl()->getQualifiedNameAsString();
  } else if (argRE->isArgumentType()) {
    QualType RT = argRE->getArgumentType();
    if (const NamedDecl *ND = ReflexprExpr::findTypeDecl(RT)) {
      return ND->getQualifiedNameAsString();
    }
    // TODO[reflexpr] can we use this ?
    // return TypeName::getFullyQualifiedName(RT, Ctx);
    // otherwise we'd need to copy its functionality here
  }
  return opGetBaseName(Ctx, moid);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetScope(ASTContext &Ctx,
                                                uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    if (const DeclContext *scopeDC = ND->getDeclContext()) {
      if (const NamedDecl *nDecl = dyn_cast<NamedDecl>(scopeDC)) {
        return ReflexprExpr::getNamedDeclReflexprExpr(Ctx, nDecl);
      }
    }
  }
  // TODO[reflexpr]

  return ReflexprExpr::getGSReflexprExpr(Ctx);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetType(ASTContext &Ctx,
                                               uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    if (const auto *DD = dyn_cast<DeclaratorDecl>(ND)) {
      TypeSourceInfo *TInfo = DD->getTypeSourceInfo();
      return ReflexprExpr::getTypeReflexprExpr(Ctx, TInfo, true);
    } else if (const DeclContext *scopeDC = ND->getDeclContext()) {
      if (const auto *ED = dyn_cast<EnumDecl>(scopeDC)) {
        return ReflexprExpr::getNamedDeclReflexprExpr(Ctx, ED);
      }
    }
  }
  // TODO[reflexpr]
  llvm_unreachable("Failed to get type!");

  return argRE;
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetAliased(ASTContext &Ctx,
                                                  uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);

  if (argRE->isArgumentType()) {
    QualType RT = argRE->getArgumentType();
    if (const auto *STTPT = dyn_cast<SubstTemplateTypeParmType>(RT)) {
      QualType Ty = STTPT->getReplacementType();
      return ReflexprExpr::getTypeReflexprExpr(Ctx, Ty, true);
    } else if (const auto *TDT = dyn_cast<TypedefType>(RT)) {
      QualType Ty = TDT->desugar();
      return ReflexprExpr::getTypeReflexprExpr(Ctx, Ty, true);
    }
  }

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(Ctx)) {
    if (const auto *TDND = dyn_cast<TypedefNameDecl>(ND)) {
      QualType Ty = TDND->getUnderlyingType();
      return ReflexprExpr::getTypeReflexprExpr(Ctx, Ty, true);
    } else if (const auto *TD = dyn_cast<TypeDecl>(ND)) {
      QualType Ty(TD->getTypeForDecl(), 0);
      return ReflexprExpr::getTypeReflexprExpr(Ctx, Ty, true);
    } else if (const auto *NsAD = dyn_cast<NamespaceAliasDecl>(ND)) {
      const NamespaceDecl *NsD = NsAD->getNamespace();
      return ReflexprExpr::getNamedDeclReflexprExpr(Ctx, NsD);
    }
  }
  // TODO[reflexpr]
  llvm_unreachable("Failed to get aliased declaration or type!");

  return argRE;
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetTagSpecifier(ASTContext &C,
                                                       uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(C, moid);

  if (const NamedDecl *ND = argRE->findArgumentNamedDecl(C, true)) {
    if (const TagDecl *TD = dyn_cast<TagDecl>(ND)) {
      switch (TD->getTagKind()) {
      case TTK_Enum:
        return ReflexprExpr::getSpecifierReflexprExpr(C, tok::kw_enum);
      case TTK_Union:
        return ReflexprExpr::getSpecifierReflexprExpr(C, tok::kw_union);
      case TTK_Class:
        return ReflexprExpr::getSpecifierReflexprExpr(C, tok::kw_class);
      case TTK_Struct:
        return ReflexprExpr::getSpecifierReflexprExpr(C, tok::kw_struct);
      case TTK_Interface:
        return ReflexprExpr::getSpecifierReflexprExpr(C, tok::kw___interface);
      }
    }
  }
  return ReflexprExpr::getNoSpecifierReflexprExpr(C);
}

bool UnaryMetaobjectOpExpr::opIsEnum(ASTContext &Ctx, uintptr_t moid) {
  if (const auto *ND =
          asReflexpr(Ctx, moid)->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *TD = dyn_cast<TagDecl>(ND))
      return TD->getTagKind() == TTK_Enum;
  }
  return false;
}

bool UnaryMetaobjectOpExpr::opIsClass(ASTContext &Ctx, uintptr_t moid) {
  if (const auto *ND =
          asReflexpr(Ctx, moid)->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *TD = dyn_cast<TagDecl>(ND))
      return TD->getTagKind() == TTK_Class;
  }
  return false;
}

bool UnaryMetaobjectOpExpr::opIsStruct(ASTContext &Ctx, uintptr_t moid) {
  if (const auto *ND =
          asReflexpr(Ctx, moid)->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *TD = dyn_cast<TagDecl>(ND))
      return TD->getTagKind() == TTK_Struct;
  }
  return false;
}

bool UnaryMetaobjectOpExpr::opIsUnion(ASTContext &Ctx, uintptr_t moid) {
  if (const auto *ND =
          asReflexpr(Ctx, moid)->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *TD = dyn_cast<TagDecl>(ND))
      return TD->getTagKind() == TTK_Union;
  }
  return false;
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetAccessSpecifier(ASTContext &Ctx,
                                                          uintptr_t moid) {

  switch (getArgumentAccess(Ctx, moid)) {
  case AS_public:
    return ReflexprExpr::getSpecifierReflexprExpr(Ctx, tok::kw_public);
  case AS_protected:
    return ReflexprExpr::getSpecifierReflexprExpr(Ctx, tok::kw_protected);
  case AS_private:
    return ReflexprExpr::getSpecifierReflexprExpr(Ctx, tok::kw_private);
  case AS_none:
    break;
  }
  return ReflexprExpr::getNoSpecifierReflexprExpr(Ctx);
}

bool UnaryMetaobjectOpExpr::opIsPublic(ASTContext &Ctx, uintptr_t moid) {
  return getArgumentAccess(Ctx, moid) == AS_public;
}
bool UnaryMetaobjectOpExpr::opIsProtected(ASTContext &Ctx, uintptr_t moid) {
  return getArgumentAccess(Ctx, moid) == AS_protected;
}
bool UnaryMetaobjectOpExpr::opIsPrivate(ASTContext &Ctx, uintptr_t moid) {
  return getArgumentAccess(Ctx, moid) == AS_private;
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetBaseClasses(ASTContext &Ctx,
                                                      uintptr_t moid) {
  return ReflexprExpr::getSeqReflexprExpr(Ctx, asReflexpr(Ctx, moid),
                                          MOSK_BaseClasses);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetMemberTypes(ASTContext &Ctx,
                                                      uintptr_t moid) {
  // TODO[reflexpr] check if operation is applicable
  return ReflexprExpr::getSeqReflexprExpr(Ctx, asReflexpr(Ctx, moid),
                                          MOSK_MemberTypes);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetMemberVariables(ASTContext &Ctx,
                                                          uintptr_t moid) {
  // TODO[reflexpr] check if operation is applicable
  return ReflexprExpr::getSeqReflexprExpr(Ctx, asReflexpr(Ctx, moid),
                                          MOSK_MemberVariables);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetMemberConstants(ASTContext &Ctx,
                                                          uintptr_t moid) {
  // TODO[reflexpr] check if operation is applicable
  return ReflexprExpr::getSeqReflexprExpr(Ctx, asReflexpr(Ctx, moid),
                                          MOSK_MemberConstants);
}

ReflexprExpr *UnaryMetaobjectOpExpr::opGetBaseClass(ASTContext &Ctx,
                                                    uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  assert(argRE->isArgumentBaseSpecifier());
  const CXXBaseSpecifier *BS = argRE->getArgumentBaseSpecifier();
  return ReflexprExpr::getTypeReflexprExpr(Ctx, BS->getTypeSourceInfo(), true);
}

bool UnaryMetaobjectOpExpr::opIsVirtual(ASTContext &Ctx, uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentBaseSpecifier()) {
    return argRE->getArgumentBaseSpecifier()->isVirtual();
  }
  return false;
}

bool UnaryMetaobjectOpExpr::opIsScopedEnum(ASTContext &Ctx, uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (argRE->isArgumentNamedDecl()) {
    if (const auto *ED = dyn_cast<EnumDecl>(argRE->getArgumentNamedDecl()))
      return ED->isScoped();
  }
  return true;
}

bool UnaryMetaobjectOpExpr::opIsStatic(ASTContext &Ctx, uintptr_t moid) {
  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  if (const auto *ND = argRE->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *VD = dyn_cast<VarDecl>(ND))
      return VD->isStaticDataMember() || VD->isStaticLocal();
  }
  return false;
}

ReflexprExpr *UnaryMetaobjectOpExpr::opHideProtected(ASTContext &Ctx,
                                                     uintptr_t moid) {
  return ReflexprExpr::getHideProtectedReflexprExpr(Ctx, asReflexpr(Ctx, moid));
}

ReflexprExpr *UnaryMetaobjectOpExpr::opHidePrivate(ASTContext &Ctx,
                                                   uintptr_t moid) {
  return ReflexprExpr::getHidePrivateReflexprExpr(Ctx, asReflexpr(Ctx, moid));
}

template <typename Action>
static void applyOnMetaobjSeqElements(ASTContext &Ctx, Action &act,
                                      ReflexprExpr *argRE) {
  assert(argRE->getKind() == MOK_ObjectSequence);

  bool hidePriv = argRE->getHidePrivate();
  bool hideProt = argRE->getHideProtected();

  if (const auto *ND = argRE->findArgumentNamedDecl(Ctx, true)) {
    if (const auto *DC = dyn_cast<DeclContext>(ND)) {
      if (argRE->getSeqKind() == MOSK_MemberTypes) {
        auto matches = [](const Decl *d) -> bool {
          if (isa<CXXRecordDecl>(d) && d->isImplicit()) {
            return false;
          }
          return isa<TypeDecl>(d);
        };
        act(matches, DC->decls_begin(), DC->decls_end(), hidePriv, hideProt);
      } else if (argRE->getSeqKind() == MOSK_MemberVariables) {
        auto matches = [](const Decl *d) -> bool {
          return isa<FieldDecl>(d) || isa<VarDecl>(d);
        };
        act(matches, DC->decls_begin(), DC->decls_end(), hidePriv, hideProt);
      } else if (argRE->getSeqKind() == MOSK_MemberConstants) {
        auto matches = [](const Decl *d) -> bool {
          return isa<EnumConstantDecl>(d);
        };
        act(matches, DC->decls_begin(), DC->decls_end(), hidePriv, hideProt);
      } else if (argRE->getSeqKind() == MOSK_BaseClasses) {
        if (const auto *RD = dyn_cast<CXXRecordDecl>(ND)) {
          auto matches = [](const CXXBaseSpecifier &) -> bool { return true; };
          act(matches, RD->bases_begin(), RD->bases_end(), hidePriv, hideProt);
        }
      }
    }
  }
}

struct matchingMetaobjSeqElementUtils {
  static bool is_private(const Decl *x) { return x->getAccess() == AS_private; }
  static bool is_private(const CXXBaseSpecifier &x) {
    return x.getAccessSpecifier() == AS_private;
  }

  static bool is_protected(const Decl *x) {
    return x->getAccess() == AS_protected;
  }
  static bool is_protected(const CXXBaseSpecifier &x) {
    return x.getAccessSpecifier() == AS_protected;
  }
};

struct countMatchingMetaobjSeqElements : matchingMetaobjSeqElementUtils {
  unsigned count;

  countMatchingMetaobjSeqElements(unsigned c) : count(c) {}

  template <typename Predicate, typename Iter>
  void operator()(Predicate &matches, Iter i, Iter e, bool hideProtected,
                  bool hidePrivate) {

    while (i != e) {
      if (matches(*i)) {
        if (is_private(*i)) {
          if (!hidePrivate)
            ++count;
        } else if (is_protected(*i)) {
          if (!hideProtected)
            ++count;
        } else {
          ++count;
        }
      }
      ++i;
    }
  }
};

unsigned UnaryMetaobjectOpExpr::opGetSize(ASTContext &Ctx, uintptr_t moid) {

  countMatchingMetaobjSeqElements action(0u);
  applyOnMetaobjSeqElements(Ctx, action, asReflexpr(Ctx, moid));
  return action.count;
}

struct findMatchingMetaobjSeqElement : matchingMetaobjSeqElementUtils {
  unsigned index;
  union {
    void *rptr;
    const Decl *decl;
    const CXXBaseSpecifier *base;
  } result;

  void storeResult(const Decl *d) { result.decl = d; }
  void storeResult(const CXXBaseSpecifier &b) { result.base = &b; }

  findMatchingMetaobjSeqElement(unsigned idx) : index(idx) {
    result.rptr = nullptr;
  }

  template <typename Predicate, typename Iter>
  void operator()(Predicate &matches, Iter i, Iter e, bool hideProtected,
                  bool hidePrivate) {
    while (i != e) {
      bool does_match = false;
      if (matches(*i)) {
        does_match = (is_private(*i) && !hidePrivate) ||
                     (is_protected(*i) && !hideProtected) ||
                     (!is_private(*i) && !is_protected(*i));
      }
      if (does_match) {
        if (index == 0)
          break;
        --index;
      }
      ++i;
    }
    assert((index == 0) && "Metaobject sequence index out of range");
    storeResult(*i);
  }
};

ReflexprExpr *NaryMetaobjectOpExpr::opGetElement(ASTContext &Ctx,
                                                 uintptr_t moid, unsigned idx) {

  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  findMatchingMetaobjSeqElement action(idx);
  applyOnMetaobjSeqElements(Ctx, action, argRE);
  assert(action.result.decl || action.result.base);

  if (argRE->getSeqKind() == MOSK_BaseClasses) {
    const CXXBaseSpecifier *BS = action.result.base;
    assert(BS != nullptr);

    return ReflexprExpr::getBaseSpecifierReflexprExpr(Ctx, BS);
  } else {
    const NamedDecl *ND = dyn_cast<NamedDecl>(action.result.decl);
    assert(ND != nullptr);

    return ReflexprExpr::getNamedDeclReflexprExpr(Ctx, ND);
  }
}

struct collectMatchingMetaobjSeqElements : matchingMetaobjSeqElementUtils {
  std::vector<const void *> elements;

  void pushElement(const Decl *d) { elements.push_back(d); }
  void pushElement(const CXXBaseSpecifier &b) { elements.push_back(&b); }

  collectMatchingMetaobjSeqElements(void) { elements.reserve(8); }

  template <typename Predicate, typename Iter>
  void operator()(Predicate matches, Iter i, Iter e, bool hideProtected,
                  bool hidePrivate) {

    while (i != e) {
      if (matches(*i)) {
        if (is_private(*i)) {
          if (!hidePrivate)
            pushElement(*i);
        } else if (is_protected(*i)) {
          if (!hideProtected)
            pushElement(*i);
        } else {
          pushElement(*i);
        }
      }
      ++i;
    }
  }
};

void UnaryMetaobjectOpExpr::unpackSequence(ASTContext &Ctx, uintptr_t moid,
                                           std::vector<llvm::APSInt> &dest) {

  ReflexprExpr *argRE = asReflexpr(Ctx, moid);
  collectMatchingMetaobjSeqElements action;
  applyOnMetaobjSeqElements(Ctx, action, argRE);

  dest.reserve(dest.size() + action.elements.size());

  if (argRE->getSeqKind() == MOSK_BaseClasses) {
    for (const void *E : action.elements) {
      const CXXBaseSpecifier *B = static_cast<const CXXBaseSpecifier *>(E);
      assert(B != nullptr);

      ReflexprExpr *RE = ReflexprExpr::getBaseSpecifierReflexprExpr(Ctx, B);
      dest.push_back(makeMetaobjResult(Ctx, RE));
    }
  } else {
    for (const void *E : action.elements) {
      const Decl *D = static_cast<const Decl *>(E);
      const NamedDecl *ND = dyn_cast<NamedDecl>(D);
      assert(ND != nullptr);

      ReflexprExpr *RE = ReflexprExpr::getNamedDeclReflexprExpr(Ctx, ND);
      dest.push_back(makeMetaobjResult(Ctx, RE));
    }
  }
}

llvm::APSInt UnaryMetaobjectOpExpr::getIntResult(ASTContext &Ctx,
                                                 UnaryMetaobjectOp MoOp,
                                                 uintptr_t moid) {
  switch (MoOp) {
#define METAOBJECT_INT_OP_1(S, OpRes, OpName, K)                               \
  case UMOO_##OpName:                                                          \
    return make##OpRes##Result(Ctx, op##OpName(Ctx, moid));
#include "clang/Basic/TokenKinds.def"

#define METAOBJECT_TRAIT(S, Concept, K) case UMOO_IsMeta##Concept:
#include "clang/Basic/TokenKinds.def"
    {
      MetaobjectKind MoK = asReflexpr(Ctx, moid)->getKind();
      MetaobjectConcept MoC = translateMetaobjectKindToMetaobjectConcept(MoK);
      return makeBoolResult(Ctx, getTraitValue(MoOp, MoC));
    }
  case UMOO_GetPointer:
  case UMOO_GetBaseName:
  case UMOO_GetDisplayName:
  case UMOO_GetSourceFile: {
    llvm_unreachable("This metaobject operation does not return int value!");
  }
  }
  llvm_unreachable("Metaobject operation not implemented yet!");
}

bool UnaryMetaobjectOpExpr::getIntResult(ASTContext &Ctx, void *EvlInfo,
                                         llvm::APSInt &result) const {
  uintptr_t moid1;
  if (!queryArgMetaobjectId(Ctx, EvlInfo, moid1)) {
    return false;
  }
  result = getIntResult(Ctx, getKind(), moid1);
  return true;
}

std::string UnaryMetaobjectOpExpr::getStrResult(ASTContext &Ctx,
                                                UnaryMetaobjectOp MoOp,
                                                uintptr_t moid) {
  switch (MoOp) {
#define METAOBJECT_STR_OP_1(S, OpRes, OpName, K)                               \
  case UMOO_##OpName:                                                          \
    return op##OpName(Ctx, moid);
#include "clang/Basic/TokenKinds.def"
  default: {
    llvm_unreachable("This metaobject operation does not return a string!");
  }
  }
}

bool UnaryMetaobjectOpExpr::getStrResult(ASTContext &Ctx,
                                         UnaryMetaobjectOp MoOp, void *EvlInfo,
                                         Expr *argExpr, std::string &result) {
  uintptr_t moid;
  if (!queryExprMetaobjectId(Ctx, moid, EvlInfo, argExpr)) {
    return false;
  }
  result = getStrResult(Ctx, MoOp, moid);
  return true;
}

const ValueDecl *UnaryMetaobjectOpExpr::getValueDeclResult(
    ASTContext &Ctx, UnaryMetaobjectOp MoOp, uintptr_t moid) {

  switch (MoOp) {
  case UMOO_GetPointer:
  case UMOO_GetConstant: {
    ReflexprExpr *argRE = asReflexpr(Ctx, moid);
    return argRE->findArgumentValueDecl(Ctx);
  }
  default:
    break;
  }
  llvm_unreachable("Failed to get value declaration from metaobject!");
}

const ValueDecl *UnaryMetaobjectOpExpr::getValueDeclResult(
    ASTContext &Ctx, UnaryMetaobjectOp MoOp, void *EvlInfo, Expr *argExpr) {
  uintptr_t moid;
  if (!queryExprMetaobjectId(Ctx, moid, EvlInfo, argExpr)) {
    llvm_unreachable("Failed to query Metaobject information!");
  }
  return getValueDeclResult(Ctx, MoOp, moid);
}

bool UnaryMetaobjectOpExpr::hasOpResultType() const {
  switch (getKind()) {
  case UMOO_GetSourceLine:
  case UMOO_GetSourceColumn:
  case UMOO_GetPointer:
  case UMOO_GetConstant:
    return true;
  default:
    break;
  }
  return false;
}

QualType UnaryMetaobjectOpExpr::getValueDeclType(ASTContext &Ctx,
                                                 UnaryMetaobjectOp MoOp,
                                                 const ValueDecl *valDecl) {
  assert(valDecl != nullptr);

  QualType result;

  if (MoOp == UMOO_GetPointer) {
    const VarDecl *varDecl = dyn_cast<VarDecl>(valDecl);
    const FieldDecl *fldDecl = dyn_cast<FieldDecl>(valDecl);

    if (varDecl) {
      result = Ctx.getPointerType(valDecl->getType());
    } else if (fldDecl) {
      const RecordDecl *RD = fldDecl->getParent();
      QualType RecTy = Ctx.getRecordType(RD);
      result = Ctx.getMemberPointerType(valDecl->getType(), RecTy.getTypePtr());
    }
  } else if (MoOp == UMOO_GetConstant) {
    return valDecl->getType();
  }
  return result;
}

QualType UnaryMetaobjectOpExpr::getOpResultType(ASTContext &Context) const {
  UnaryMetaobjectOp MoOp = getKind();
  switch (MoOp) {
  case UMOO_GetSourceLine:
  case UMOO_GetSourceColumn: {
    return Context.UnsignedIntTy;
  }
  case UMOO_GetConstant:
  case UMOO_GetPointer: {
    const auto *VD =
        getValueDeclResult(Context, MoOp, nullptr, getArgumentExpr());
    return getValueDeclType(Context, MoOp, VD);
  }
  default:
    break;
  }
  // TODO[reflexpr] diagnostic
  return QualType();
}

Stmt::child_range UnaryMetaobjectOpExpr::children() {
  return child_range(child_iterator(&ArgExpr + 0),
                     child_iterator(&ArgExpr + 1));
}

NaryMetaobjectOpExpr::NaryMetaobjectOpExpr(ASTContext &Ctx,
                                           NaryMetaobjectOp Oper,
                                           MetaobjectOpResult OpRes,
                                           QualType resultType, unsigned arity,
                                           Expr **argExpr, SourceLocation opLoc,
                                           SourceLocation endLoc)
    : Expr(NaryMetaobjectOpExprClass, resultType, VK_RValue, OK_Ordinary,
           _anyTypeDependent(arity, argExpr),
           _anyValueDependent(arity, argExpr),
           _anyInstDependent(arity, argExpr),
           _anyHasUnexpandedPack(arity, argExpr)),
      OpLoc(opLoc), EndLoc(endLoc) {

  for (unsigned i = 0; i < arity; ++i) {
    ArgExpr[i] = argExpr[i];
  }
  for (unsigned i = arity; i < MaxArity; ++i) {
    ArgExpr[i] = nullptr;
  }

  setKind(Oper);
  setResultKind(OpRes);
}

StringRef NaryMetaobjectOpExpr::getOperationSpelling(NaryMetaobjectOp MoOp) {
  switch (MoOp) {
#define METAOBJECT_OP_2(Spelling, R, Name, K)                                  \
  case NMOO_##Name:                                                            \
    return #Spelling;
#include "clang/Basic/TokenKinds.def"
  }
  return StringRef();
}

bool NaryMetaobjectOpExpr::isOperationApplicable(MetaobjectKind MoK,
                                                 NaryMetaobjectOp MoOp) {

  MetaobjectConcept MoC = translateMetaobjectKindToMetaobjectConcept(MoK);
  switch (MoOp) {
  case NMOO_ReflectsSame:
    return true;
  case NMOO_GetElement:
    return conceptIsA(MoC, MOC_ObjectSequence);
  }
  return false;
}

bool NaryMetaobjectOpExpr::opReflectsSame(ASTContext &Ctx, uintptr_t moid1,
                                          uintptr_t moid2) {
  if (moid1 == moid2)
    return true;

  ReflexprExpr *argRE1 = asReflexpr(Ctx, moid1);
  ReflexprExpr *argRE2 = asReflexpr(Ctx, moid2);
  if (argRE1->isArgumentGlobalScope() && argRE2->isArgumentGlobalScope())
    return true;

  if (argRE1->isArgumentNoSpecifier() && argRE2->isArgumentNoSpecifier())
    return true;

  if (argRE1->isArgumentSpecifier() && argRE2->isArgumentSpecifier()) {
    return argRE1->getArgumentSpecifierKind() ==
           argRE2->getArgumentSpecifierKind();
  }

  if (argRE1->isArgumentNamedDecl() && argRE2->isArgumentNamedDecl()) {
    auto ND1 = argRE1->getArgumentNamedDecl();
    auto ND2 = argRE2->getArgumentNamedDecl();
    if (ND1 == ND2)
      return true;
    if (ND1->getDeclName() == ND2->getDeclName()) {
      if (ND1->getDeclContext()->getRedeclContext()->Equals(
              ND2->getDeclContext()->getRedeclContext())) {
        if (ND1->getKind() == ND2->getKind()) {
          // TODO[reflexpr]
          return true;
        }
      }
    }
  }

  if (argRE1->isArgumentType() && argRE2->isArgumentType()) {
    auto Ty1 = argRE1->getArgumentType();
    auto Ty2 = argRE2->getArgumentType();

    if (Ctx.hasSameType(Ty1, Ty2)) {
      return true;
    }
  }
  // TODO[reflexpr]
  return false;
}

bool NaryMetaobjectOpExpr::getIntResult(ASTContext &Ctx, void *EvlInfo,
                                        llvm::APSInt &result) const {
  uintptr_t moid1;
  if (!queryArgMetaobjectId(Ctx, EvlInfo, moid1, 0)) {
    return false;
  }

  switch (getKind()) {
  case NMOO_ReflectsSame: {
    uintptr_t moid2;
    if (!queryArgMetaobjectId(Ctx, EvlInfo, moid2, 1)) {
      return false;
    }
    result = makeBoolResult(Ctx, opReflectsSame(Ctx, moid1, moid2));
    return true;
  }
  case NMOO_GetElement: {
    uint64_t index;
    if (!queryArgUIntValue(Ctx, index, 1)) {
      return false;
    }
    result = makeMetaobjResult(Ctx, opGetElement(Ctx, moid1, index));
    return true;
  }
  }
  return false;
}

Stmt::child_range NaryMetaobjectOpExpr::children() {
  return child_range(child_iterator(ArgExpr + 0),
                     child_iterator(ArgExpr + MaxArity));
}
