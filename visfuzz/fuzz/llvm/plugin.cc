#include <unordered_map>
#include <vector>

#include "llvm/IR/CFG.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <iostream>
#include <queue>
#include <vector>

using namespace std;
using namespace llvm;

static cl::opt<std::string>
    VisFilename("fvisfuzz-export",
                cl::desc("Specify output filename for VisFuzz"),
                cl::value_desc("filename"));

namespace {

struct VisBasicBlock {
        unsigned Line;
        unsigned LineEnd;
        unsigned Function;
        std::vector<unsigned> Successors;

        VisBasicBlock(BasicBlock &BB, unsigned Function)
            : Line(-1u), LineEnd(0), Function(Function) {
                for (auto &Inst : BB) {
                        if (auto Loc = Inst.getDebugLoc()) {
                                if (Loc.getLine() == 0)
                                        continue;
                                Line = std::min(Line, Loc.getLine());
                                LineEnd = std::max(LineEnd, Loc.getLine());
                        }
                }
                // Restore to 0 when debug info is not available
                if (Line == -1u)
                        Line = 0;
        }
};

struct VisFunction {
        StringRef Name;
        StringRef Filename;
        unsigned Line;
        unsigned LineEnd;
        unsigned EntryBB;
        unsigned BBSum;
        std::vector<unsigned> Calls;
        std::vector<unsigned> Refs;

        VisFunction(Function &F)
            : Line(0), LineEnd(0), EntryBB(-1u), BBSum(0u) {
                Name = F.getName();
                auto Subprog = F.getSubprogram();
                if (Subprog) {
                        Name = Subprog->getName();
                        Filename = Subprog->getFilename();
                        Line = Subprog->getLine();
                }

                for (auto &BB : F) {
                        auto end = VisBasicBlock(BB, 0).LineEnd;
                        LineEnd = std::max(end, LineEnd);
                }
        }
};

class VisFuzz : public FunctionPass {
      public:
        static char ID;

        VisFuzz() : FunctionPass(ID) {}

        ~VisFuzz() {
                vector<unsigned> visited(BasicBlocks.size());
                for (int i = 0; i < Functions.size(); i++) {
                        if (Functions[i].EntryBB != -1u) {
                                std::fill(visited.begin(), visited.end(), 0);
                                Functions[i].BBSum = CalculateBBSum(
                                    visited, Functions[i].EntryBB);
                        }
                }
                auto FunctionEntries = json::Array();
                auto FunctionInserter = std::back_inserter(FunctionEntries);
                std::transform(Functions.begin(), Functions.end(),
                               FunctionInserter, [](VisFunction &F) {
                                       auto CallEntries = json::Array();
                                       auto CallEntryInserter =
                                           std::back_inserter(CallEntries);
                                       std::copy(F.Calls.begin(), F.Calls.end(),
                                                 CallEntryInserter);

                                       auto RefEntries = json::Array();
                                       auto RefEntryInserter =
                                           std::back_inserter(RefEntries);
                                       std::copy(F.Refs.begin(), F.Refs.end(),
                                                 RefEntryInserter);

                                       auto FEntry = json::Object();
                                       FEntry["name"] = F.Name;
                                       FEntry["block_sum"] = F.BBSum;
                                       FEntry["calls"] = std::move(CallEntries);
                                       FEntry["refs"] = std::move(RefEntries);
                                       if (F.Line != 0)
                                               FEntry["line"] = F.Line;
                                       if (F.LineEnd != 0)
                                               FEntry["line_end"] = F.LineEnd;
                                       if (F.EntryBB != -1u)
                                               FEntry["entry_block"] =
                                                   std::move(F.EntryBB);
                                       if (!F.Filename.empty())
                                               FEntry["filename"] = F.Filename;
                                       return json::Value(std::move(FEntry));
                               });

                auto BBEntries = json::Array();
                auto BBEntryInserter = std::back_inserter(BBEntries);
                std::transform(BasicBlocks.begin(), BasicBlocks.end(),
                               BBEntryInserter, [](VisBasicBlock &BB) {
                                       auto SuccessorEntries = json::Array();
                                       auto SuccessorEntryInserter =
                                           std::back_inserter(SuccessorEntries);
                                       std::copy(BB.Successors.begin(),
                                                 BB.Successors.end(),
                                                 SuccessorEntryInserter);

                                       auto BBEntry = json::Object();
                                       if (BB.Line != 0)
                                               BBEntry["line"] = BB.Line;
                                       if (BB.LineEnd != 0)
                                               BBEntry["line_end"] = BB.LineEnd;
                                       if (BB.Function != -1u)
                                               BBEntry["function"] =
                                                   BB.Function;
                                       BBEntry["successors"] =
                                           std::move(SuccessorEntries);
                                       return BBEntry;
                               });

                auto RootObject = json::Object();
                RootObject["basic_blocks"] = std::move(BBEntries);
                RootObject["functions"] = std::move(FunctionEntries);
                auto Json = json::Value(std::move(RootObject));

                if (VisFilename.empty()) {
                        errs() << formatv("{0:2}", Json) << '\n';
                } else {
                        auto ec = std::error_code();
                        raw_fd_ostream file(VisFilename, ec);
                        if (file.has_error()) {
                                errs() << "Can't open file " << VisFilename;
                                return;
                        }
                        file << Json;
                }
        }

      private:
        std::vector<VisFunction> Functions;
        std::vector<VisBasicBlock> BasicBlocks;
        std::unordered_map<BasicBlock *, unsigned> BasicBlockMap;
        std::unordered_map<Function *, unsigned> FunctionMap;

        GlobalVariable *AFLMapPtr;
        unsigned NoSanitize;

        unsigned GenerateFunctionSummary(Function &F) {
                auto it = FunctionMap.find(&F);
                if (it == FunctionMap.end()) {
                        auto n = Functions.size();
                        Functions.push_back(VisFunction(F));
                        FunctionMap[&F] = n;
                        return n;
                } else {
                        return it->second;
                }
        }
        unsigned CalculateBBSum(vector<unsigned> &visited, unsigned BB) {
                unsigned count = 0;
                visited[BB] = 1;
                queue<unsigned> q;
                q.push(BB);
                while (!q.empty()) {
                        auto nowBB = q.front();
                        q.pop();
                        count++;
                        for (auto i : BasicBlocks[nowBB].Successors) {
                                if (!visited[i]) {
                                        q.push(i);
                                        visited[i] = 1;
                                }
                        }
                }
                return count;
        }
        void GenerateCalls(Function &F, unsigned ThisIndex) {
                for (auto &BB : F) {
                        for (auto &I : BB.instructionsWithoutDebug()) {
                                if (auto CS = CallSite(&I)) {
                                        // Direct call: save to
                                        // "Calls"
                                        if (auto Callee =
                                                CS.getCalledFunction()) {
                                                auto ThatIndex =
                                                    GenerateFunctionSummary(
                                                        *Callee);
                                                Functions[ThisIndex]
                                                    .Calls.push_back(ThatIndex);
                                        }
                                } else {
                                        // Involved in operands: save
                                        // to "Refs"
                                        for (auto Op : I.operand_values()) {
                                                if (auto *Callee =
                                                        dyn_cast<Function>(
                                                            Op)) {
                                                        auto ThatIndex =
                                                            GenerateFunctionSummary(
                                                                *Callee);
                                                        Functions[ThisIndex]
                                                            .Refs.push_back(
                                                                ThatIndex);
                                                }
                                        }
                                }
                        }
                }
        }

        unsigned GenerateCFG(Function &F, unsigned ThisIndex) {
                auto &EntryBB = F.getEntryBlock();
                auto EntryBBIndex = BasicBlocks.size();
                BasicBlocks.emplace_back(EntryBB, ThisIndex);
                auto BBPtrs = std::vector<BasicBlock *>{&EntryBB};
                BasicBlockMap[&EntryBB] = EntryBBIndex;

                for (auto I = 0u; I < BBPtrs.size(); ++I) {
                        auto BB = BBPtrs[I];
                        auto BBIndex = BasicBlockMap[BB];
                        for (auto Successor : successors(BB)) {
                                auto It = BasicBlockMap.find(Successor);
                                auto SuccessorIndex = -1u;
                                if (It == BasicBlockMap.end()) {
                                        SuccessorIndex = BasicBlocks.size();
                                        BBPtrs.push_back(Successor);
                                        BasicBlocks.emplace_back(*Successor,
                                                                 ThisIndex);
                                        BasicBlockMap[Successor] =
                                            SuccessorIndex;
                                } else {
                                        SuccessorIndex = It->second;
                                }

                                BasicBlocks[BBIndex].Successors.emplace_back(
                                    SuccessorIndex);
                        }
                }

                return EntryBBIndex;
        }

        void SetNoSanitize(Instruction *I) {
                auto &C = I->getContext();
                I->setMetadata(NoSanitize, MDNode::get(C, None));
        }

        void Instrument(Function &F) {
                auto &C = F.getContext();
                auto Int16Ty = IntegerType::getInt16Ty(C);
                auto Int32Ty = IntegerType::getInt32Ty(C);

                for (auto &BB : F) {
                        auto BBIndex = BasicBlockMap[&BB];
                        auto IRB = IRBuilder<>(&(*BB.getFirstInsertionPt()));
                        auto MapPtr = IRB.CreateLoad(AFLMapPtr);
                        auto MapPtrIdx = IRB.CreateGEP(
                            MapPtr, ConstantInt::get(Int32Ty, BBIndex));
                        auto Counter = IRB.CreateLoad(MapPtrIdx);
                        auto Incr = IRB.CreateAdd(Counter,
                                                  ConstantInt::get(Int16Ty, 1));
                        SetNoSanitize(Counter);
                        auto Store = IRB.CreateStore(Incr, MapPtrIdx);
                        SetNoSanitize(Store);
                }
        }

      protected:
        bool doInitialization(Module &M) override {
                auto &C = M.getContext();
                auto Int16Ty = IntegerType::getInt16Ty(C);
                if (!AFLMapPtr)
                        delete AFLMapPtr;
                AFLMapPtr = new GlobalVariable(
                    M, PointerType::get(Int16Ty, 0), false,
                    GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");
                NoSanitize = C.getMDKindID("nosanitize");
                return false;
        }

        bool runOnFunction(Function &F) override {
                auto I = GenerateFunctionSummary(F);
                GenerateCalls(F, I);
                auto EntryBB = GenerateCFG(F, I);
                Functions[I].EntryBB = EntryBB;
                Instrument(F);
                return true;
        }
};

} // namespace

char VisFuzz::ID = 0;
static RegisterPass<VisFuzz> X("visfuzz", "VisFuzz Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);
