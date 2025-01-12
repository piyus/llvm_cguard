//===--------- Definition of the AddressSanitizer class ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the AddressSanitizer class which is a port of the legacy
// AddressSanitizer pass to use the new PassManager infrastructure.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_FAST_ADDRESSSANITIZERPASS_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_FAST_ADDRESSSANITIZERPASS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

/// Frontend-provided metadata for source location.
struct FastLocationMetadata {
  StringRef Filename;
  int LineNo = 0;
  int ColumnNo = 0;

  FastLocationMetadata() = default;

  bool empty() const { return Filename.empty(); }
  void parse(MDNode *MDN);
};

/// Frontend-provided metadata for global variables.
class FastGlobalsMetadata {
public:
  struct Entry {
    FastLocationMetadata SourceLoc;
    StringRef Name;
    bool IsDynInit = false;
    bool IsBlacklisted = false;

    Entry() = default;
  };

  /// Create a default uninitialized GlobalsMetadata instance.
  FastGlobalsMetadata() = default;

  /// Create an initialized GlobalsMetadata instance.
  FastGlobalsMetadata(Module &M);

  /// Returns metadata entry for a given global.
  Entry get(GlobalVariable *G) const {
    auto Pos = Entries.find(G);
    return (Pos != Entries.end()) ? Pos->second : Entry();
  }

  /// Handle invalidation from the pass manager.
  /// These results are never invalidated.
  bool invalidate(Module &, const PreservedAnalyses &,
                  ModuleAnalysisManager::Invalidator &) {
    return false;
  }
  bool invalidate(Function &, const PreservedAnalyses &,
                  FunctionAnalysisManager::Invalidator &) {
    return false;
  }

private:
  DenseMap<GlobalVariable *, Entry> Entries;
};

/// The ASanGlobalsMetadataAnalysis initializes and returns a GlobalsMetadata
/// object. More specifically, ASan requires looking at all globals registered
/// in 'llvm.asan.globals' before running, which only depends on reading module
/// level metadata. This analysis is required to run before running the
/// AddressSanitizerPass since it collects that metadata.
/// The legacy pass manager equivalent of this is ASanGlobalsMetadataLegacyPass.
class ASanFastGlobalsMetadataAnalysis
    : public AnalysisInfoMixin<ASanFastGlobalsMetadataAnalysis> {
public:
  using Result = FastGlobalsMetadata;

  Result run(Module &, ModuleAnalysisManager &);

private:
  friend AnalysisInfoMixin<ASanFastGlobalsMetadataAnalysis>;
  static AnalysisKey Key;
};

/// Public interface to the address sanitizer pass for instrumenting code to
/// check for various memory errors at runtime.
///
/// The sanitizer itself is a function pass that works by inserting various
/// calls to the ASan runtime library functions. The runtime library essentially
/// replaces malloc() and free() with custom implementations that allow regions
/// surrounding requested memory to be checked for invalid accesses.
class FastAddressSanitizerPass : public PassInfoMixin<FastAddressSanitizerPass> {
public:
  explicit FastAddressSanitizerPass(bool CompileKernel = false,
                                    bool Recover = false,
                                    bool UseAfterScope = false);
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

private:
  bool CompileKernel;
  bool Recover;
  bool UseAfterScope;
};

/// Public interface to the address sanitizer module pass for instrumenting code
/// to check for various memory errors.
///
/// This adds 'asan.module_ctor' to 'llvm.global_ctors'. This pass may also
/// run intependently of the function address sanitizer.
class ModuleFastAddressSanitizerPass
    : public PassInfoMixin<ModuleFastAddressSanitizerPass> {
public:
  explicit ModuleFastAddressSanitizerPass(bool CompileKernel = false,
                                          bool Recover = false,
                                          bool UseGlobalGC = true,
                                          bool UseOdrIndicator = false);
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  bool CompileKernel;
  bool Recover;
  bool UseGlobalGC;
  bool UseOdrIndicator;
};

// Insert AddressSanitizer (address sanity checking) instrumentation
FunctionPass *createFastAddressSanitizerFunctionPass(bool CompileKernel = false,
                                                     bool Recover = false,
                                                     bool UseAfterScope = false);
ModulePass *createModuleFastAddressSanitizerLegacyPassPass(
    bool CompileKernel = false, bool Recover = false, bool UseGlobalsGC = true,
    bool UseOdrIndicator = true);

} // namespace llvm

#endif
