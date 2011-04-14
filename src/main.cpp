// $Id$
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "UnusedHeaderFinder.h"
#include "version.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace clang;
using namespace llvm;

namespace {

const std::string PROGRAM_NAME("find-unnecessary-includes");

void
showHelp ()
{
  std::cout <<
      "USAGE: " << PROGRAM_NAME << " [options] <inputs>\n"
      "\n"
      "OPTIONS:\n"
      "  --help                  show help\n"
      "  --version               show version\n"
      "  -D<macro>[=def]         define preprocessor macro\n"
      "  -I<dir>                 add include directory\n"
      "  -c                      exit with status 1\n"
      "  -fms-extensions         enable Microsoft extensions\n"
      "\n"
      "Many clang options are also supported.  "
      "See the clang manual for more options.\n";
}

bool
handleFrontEndOptions (FrontendOptions& opt)
{
  if (opt.ShowVersion)
  {
    std::cout << PROGRAM_NAME << ' ' << FUI_VERSION
        << "\nbased on " << getClangFullVersion() << std::endl;
    return false;
  }

  if (opt.ShowHelp)
  {
    showHelp();
    return false;
  }

  if (opt.Inputs.empty() || opt.Inputs.at(0).second == "-")
  {
    showHelp();
    return false;
  }

  return true;
}

class UnnecessaryIncludeFinderAction: public ASTFrontendAction
{
  bool& foundUnnecessary_;

public:
  UnnecessaryIncludeFinderAction (bool& foundUnnecessary):
      foundUnnecessary_(foundUnnecessary)
  { }

  ASTConsumer* CreateASTConsumer (
      CompilerInstance& compiler, StringRef inputFile)
  {
    UnusedHeaderFinder* pFinder = new UnusedHeaderFinder(
        compiler.getSourceManager(), foundUnnecessary_);
    compiler.getPreprocessor().addPPCallbacks(
        pFinder->createPreprocessorCallbacks());
    return pFinder;
  }
};

}//namespace

int
main (int argc, char* argv[])
{
  CompilerInstance compiler;

  // Create diagnostics so errors while processing command line arguments can
  // be reported.
  compiler.createDiagnostics(argc, argv);

  CompilerInvocation::CreateFromArgs(
      compiler.getInvocation(),
      argv + 1,
      argv + argc,
      compiler.getDiagnostics());

  if (!handleFrontEndOptions(compiler.getFrontendOpts())) {
    return EXIT_FAILURE;
  }

  compiler.getInvocation().setLangDefaults(IK_CXX);

  if (compiler.getHeaderSearchOpts().UseBuiltinIncludes
   && compiler.getHeaderSearchOpts().ResourceDir.empty())
  {
    compiler.getHeaderSearchOpts().ResourceDir =
        CompilerInvocation::GetResourcesPath(
            argv[0], reinterpret_cast<void*>(showHelp));
  }

  if (compiler.getLangOpts().Microsoft) {
    // Kludge to allow clang to parse Microsoft headers.
    // Visual C++ does name resolution at template instantiation, but clang does
    // name resolution at template definition.  A Microsoft header defines a
    // template referencing _invalid_parameter_noinfo but is not declared at
    // that point. It is declared in the <xutility> header file, which is
    // included later.
    compiler.getPreprocessorOpts().addMacroDef(
        "_invalid_parameter_noinfo=__noop");
  }

  bool foundUnnecessary;
  UnnecessaryIncludeFinderAction action(foundUnnecessary);
  compiler.ExecuteAction(action);

  llvm_shutdown();
  return foundUnnecessary ? EXIT_FAILURE : EXIT_SUCCESS;
}
