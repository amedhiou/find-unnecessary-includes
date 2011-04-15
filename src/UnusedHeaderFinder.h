// $Id$
#ifndef UNUSEDHEADERFINDER_H
#define UNUSEDHEADERFINDER_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Token.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <stack>
#include <string>
#include <vector>

class SourceFile;

/**
 * #include directive appearing in the source code.
 */
class IncludeDirective: public llvm::RefCountedBase<IncludeDirective>
{
public:
  typedef llvm::IntrusiveRefCntPtr<IncludeDirective> Ptr;

  /** location of #include directive in source code */
  clang::SourceLocation directiveLocation_;

  /**
   * header file name as it appears in the source without surrounding delimiters
   */
  llvm::StringRef fileName_;

  /** true if angle brackets surrounded the file name in the source */
  bool angled_;

  /** header file included by #include directive */
  SourceFile* pHeader_;

  IncludeDirective(
      clang::SourceLocation hashLoc,
      llvm::StringRef fileName,
      bool angled):
    directiveLocation_(hashLoc),
    fileName_(fileName),
    angled_(angled),
    pHeader_(0)
  { }
};

class SourceVisitor
{
public:
  /**
   * Return true if traversal should continue.
   */
  virtual bool visit(SourceFile* pSource) = 0;
};

/**
 * Main source file or header file.
 */
class SourceFile: public llvm::RefCountedBase<SourceFile>
{
public:
  typedef llvm::IntrusiveRefCntPtr<SourceFile> Ptr;

  clang::FileID fileID_;

  /** #include directives appearing in the source file */
  typedef std::vector<IncludeDirective::Ptr> IncludeDirectives;
  IncludeDirectives includeDirectives_;

  /** set of header files marked as used */
  typedef llvm::DenseSet<clang::FileID> UsedHeaders;
  UsedHeaders usedHeaders_;

  SourceFile (clang::FileID fileID):
    fileID_(fileID)
  { }

  void traverse(SourceVisitor& visitor);

  /**
   * Checks if any of the header files included by this source file are used.
   */
  bool haveNestedUsedHeader(SourceFile::Ptr pParentSource);

  /**
   * Reports the header files included by this source file that are used.
   */
  void reportNestedUsedHeaders(
      SourceFile::Ptr pParentSource, clang::SourceManager& sourceManager);

  std::string format(
      clang::SourceLocation sourceLocation,
      clang::SourceManager& sourceManager);

  /**
   * Reports unnecessary #include directives in this source file.
   *
   * @return true if an unnecessary #include directive was found
   */
  bool reportUnnecessaryIncludes(clang::SourceManager& sourceManager);
};

/**
 * Finds unused header files by traversing the translation unit, and for each
 * symbol used by the main source file, marking the header file which declares
 * the symbol as used.
 */
class UnusedHeaderFinder:
    public clang::PPCallbacks,
    public clang::ASTConsumer,
    public clang::RecursiveASTVisitor<UnusedHeaderFinder>
{
  clang::SourceManager& sourceManager_;

  // map file to last #include directive that includes it 
  typedef llvm::DenseMap<const clang::FileEntry*, IncludeDirective::Ptr>
      FileToIncludeDirectiveMap;
  FileToIncludeDirectiveMap fileToIncludeDirectiveMap_;

  // map file to source
  typedef llvm::DenseMap<const clang::FileEntry*, SourceFile::Ptr>
      FileToSourceMap;
  FileToSourceMap fileToSourceMap_;

  // stack of included files
  std::stack<SourceFile::Ptr> includeStack_;

  // main source file
  SourceFile::Ptr pMainSource_;

  bool& foundUnnecessary_;

  bool isFromMainFile (clang::SourceLocation sourceLocation)
  { return sourceManager_.isFromMainFile(sourceLocation); }

  SourceFile::Ptr getSource(
      const clang::FileEntry* pFile, clang::FileID fileID);

  SourceFile::Ptr enterHeader(
      const clang::FileEntry* pFile, clang::FileID fileID);

  void markUsed(
      clang::SourceLocation declarationLocation,
      clang::SourceLocation usageLocation);

public:
  UnusedHeaderFinder (
      clang::SourceManager& sourceManager, bool& foundUnnecessary):
    sourceManager_(sourceManager),
    foundUnnecessary_(foundUnnecessary)
  { }

  /**
   * Creates object to receive notifications of preprocessor events.
   * We need to create a new object because the preprocessor will take
   * ownership of it and invoke the delete operator on it.
   */
  clang::PPCallbacks* createPreprocessorCallbacks();

  virtual void InclusionDirective(
      clang::SourceLocation hashLoc,
      const clang::Token& includeToken,
      llvm::StringRef fileName,
      bool isAngled,
      const clang::FileEntry* pFile,
      clang::SourceLocation endLoc,
      const llvm::SmallVectorImpl<char>& rawPath);

  virtual void FileChanged(
      clang::SourceLocation newLocation,
      clang::PPCallbacks::FileChangeReason reason,
      clang::SrcMgr::CharacteristicKind fileType);

  virtual void FileSkipped(
      const clang::FileEntry& file,
      const clang::Token& fileNameToken,
      clang::SrcMgr::CharacteristicKind fileType);

  virtual void MacroExpands(
      const clang::Token& nameToken, const clang::MacroInfo* pMacro);

  virtual void HandleTranslationUnit(clang::ASTContext& astContext);

  // Called when a typedef is used.
  bool VisitTypedefTypeLoc(clang::TypedefTypeLoc typeLoc);

  // Called when a enum, struct or class is used.
  bool VisitTagTypeLoc(clang::TagTypeLoc typeLoc);

  // Called when a class template is used.
  bool VisitTemplateSpecializationTypeLoc(
      clang::TemplateSpecializationTypeLoc typeLoc);
  
  // Called when a variable, function, or enum constant is used.
  bool VisitDeclRefExpr(clang::DeclRefExpr* pExpr);
  
  // Called when a class, struct, or union member is used.
  bool VisitMemberExpr(clang::MemberExpr* pExpr);

  // Called when a member function is called.
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr* pExpr);
};

#endif
