#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>

using namespace llvm;
namespace fs = std::filesystem;

cl::OptionCategory Here3Options("here3 options");
static cl::opt<std::string> InputFilename(cl::Positional, cl::Required,
                                          cl::value_desc("filename"),
                                          cl::desc("<Input executable file>"),
                                          cl::cat(Here3Options));

static cl::list<std::string> FuncNames(cl::desc("[Function  name ...]"),
                                       cl::value_desc("function"), cl::Required,
                                       cl::cat(Here3Options), cl::ConsumeAfter);

using NameOffsetPair = std::pair<StringRef, unsigned>;

static void insertPayload(StringRef fname,
                          const std::vector<NameOffsetPair> &offsets) {
  if (offsets.empty()) {
    llvm::outs() << "No offsets found, nothing to do... sorry.\n";
    return;
  }

  // Open the input file.
  const fs::path inPath(fname.str());
  std::ifstream inFile(inPath);
  if (!inFile) {
    llvm::errs() << "Error opening " << fname << '\n';
    return;
  }

  // Create an output file with a .here3.<extension>.
  fs::path outPath(inPath);
  const auto outExtension(std::string(".here3") + inPath.extension().string());
  outPath.replace_extension(outExtension);
  std::ofstream outFile(outPath);
  if (!outFile) {
    llvm::errs() << "Error creating or truncating file: " << outPath << '\n';
    return;
  }

  // Read the input file into memory.
  const auto inSize = fs::file_size(inPath);
  auto buf = new char[inSize];
  inFile.read(buf, inSize);
  if (!inFile) {
    llvm::errs() << "Error reading input file " << inPath << '\n';
    delete[] buf;
    return;
  }

  // Replace the value at each offset with the x86 INT3 interrupt instruction.
  for (const auto &pr : offsets) {
    const auto offset = pr.second;
    if (offset >= inSize) {
      llvm::errs() << "Invalid offset found. Exiting...\n";
      delete[] buf;
      return;
    }
    llvm::outs() << "[+] Instrumenting " << pr.first << '\n';
    buf[offset] = (char)0xCC;  // INT3
  }

  // Write the modified buffer to the output file.
  outFile.write(buf, inSize);
  delete[] buf;
  if (!outFile) {
    llvm::errs() << "Error writing instrumented output file.\n";
    return;
  }
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions(Here3Options);
  cl::ParseCommandLineOptions(
      argc, argv,
      "here3 - Statically instrument an x86 binary with a breakpoint.");

  std::map<std::string, unsigned> functionOffset;

  // Load the input file and get an ObjectFile instance.
  const auto bufferOrError = MemoryBuffer::getFileOrSTDIN(InputFilename);
  if (auto err = bufferOrError.getError()) return err.value();
  Expected<std::unique_ptr<object::Binary>> bin =
      object::createBinary(**bufferOrError);
  if (auto err = bin.takeError()) return -1;
  const auto obj = dyn_cast<object::ObjectFile>(&**bin);
  const auto arch = obj->getArch();
  if (obj->isRelocatableObject() ||
      (arch != llvm::Triple::x86 && arch != llvm::Triple::x86_64)) {
    llvm::errs()
        << "Error: Input file must be an x86 (32 or 64bit) executable.\n";
    return -1;
  }
  std::unique_ptr<DWARFContext> ctx = DWARFContext::create(*obj);

  // Gather offsets for each subprogram DIE.
  for (const std::unique_ptr<DWARFUnit> &cu : ctx->compile_units()) {
    for (const DWARFDebugInfoEntry &info : cu->dies()) {
      DWARFDie die(&*cu, &info);
      if (die.isSubprogramDIE()) {
        uint64_t low, high, section_idx;
        if (die.getLowAndHighPC(low, high, section_idx)) {
          if (auto sname = die.getShortName()) functionOffset[sname] = low;
          if (auto lname = die.getLinkageName()) functionOffset[lname] = low;
        }
      }
    }
  }

#ifdef DEBUG
  for (const auto &pr : functionOffset)
    llvm::outs() << "[debug] " << pr.first << '\t' << pr.second << '\n';
#endif

  // Instrument the file.
  std::vector<NameOffsetPair> offsets;
  for (const auto &str : FuncNames)
    if (const auto &kv = functionOffset.find(str); kv != functionOffset.end())
      offsets.emplace_back(kv->first, kv->second);
  insertPayload(InputFilename, offsets);

  return 0;
}
