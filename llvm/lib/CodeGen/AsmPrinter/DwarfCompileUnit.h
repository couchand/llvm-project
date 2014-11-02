//===-- llvm/CodeGen/DwarfCompileUnit.h - Dwarf Compile Unit ---*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains support for writing dwarf compile unit.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_ASMPRINTER_DWARFCOMPILEUNIT_H
#define LLVM_LIB_CODEGEN_ASMPRINTER_DWARFCOMPILEUNIT_H

#include "DwarfUnit.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DebugInfo.h"

namespace llvm {

class AsmPrinter;
class DIE;
class DwarfDebug;
class DwarfFile;
class MCSymbol;
class LexicalScope;

class DwarfCompileUnit : public DwarfUnit {
  /// The attribute index of DW_AT_stmt_list in the compile unit DIE, avoiding
  /// the need to search for it in applyStmtList.
  unsigned stmtListIndex;

  /// Skeleton unit associated with this unit.
  DwarfCompileUnit *Skeleton;

  /// A label at the start of the non-dwo section related to this unit.
  MCSymbol *SectionSym;

  /// The start of the unit within its section.
  MCSymbol *LabelBegin;

  /// GlobalNames - A map of globally visible named entities for this unit.
  StringMap<const DIE *> GlobalNames;

  /// GlobalTypes - A map of globally visible types for this unit.
  StringMap<const DIE *> GlobalTypes;

  /// \brief Construct a DIE for the given DbgVariable without initializing the
  /// DbgVariable's DIE reference.
  std::unique_ptr<DIE> constructVariableDIEImpl(const DbgVariable &DV,
                                                bool Abstract);

  bool isDwoUnit() const override;

public:
  DwarfCompileUnit(unsigned UID, DICompileUnit Node, AsmPrinter *A,
                   DwarfDebug *DW, DwarfFile *DWU);

  DwarfCompileUnit *getSkeleton() const {
    return Skeleton;
  }

  void initStmtList(MCSymbol *DwarfLineSectionSym);

  /// Apply the DW_AT_stmt_list from this compile unit to the specified DIE.
  void applyStmtList(DIE &D);

  /// getOrCreateGlobalVariableDIE - get or create global variable DIE.
  DIE *getOrCreateGlobalVariableDIE(DIGlobalVariable GV);

  /// addLabelAddress - Add a dwarf label attribute data and value using
  /// either DW_FORM_addr or DW_FORM_GNU_addr_index.
  void addLabelAddress(DIE &Die, dwarf::Attribute Attribute,
                       const MCSymbol *Label);

  /// addLocalLabelAddress - Add a dwarf label attribute data and value using
  /// DW_FORM_addr only.
  void addLocalLabelAddress(DIE &Die, dwarf::Attribute Attribute,
                            const MCSymbol *Label);

  /// addSectionDelta - Add a label delta attribute data and value.
  void addSectionDelta(DIE &Die, dwarf::Attribute Attribute, const MCSymbol *Hi,
                       const MCSymbol *Lo);

  DwarfCompileUnit &getCU() override { return *this; }

  unsigned getOrCreateSourceID(StringRef FileName, StringRef DirName) override;

  /// addRange - Add an address range to the list of ranges for this unit.
  void addRange(RangeSpan Range);

  void attachLowHighPC(DIE &D, const MCSymbol *Begin, const MCSymbol *End);

  /// addSectionLabel - Add a Dwarf section label attribute data and value.
  ///
  void addSectionLabel(DIE &Die, dwarf::Attribute Attribute,
                       const MCSymbol *Label, const MCSymbol *Sec);

  /// \brief Find DIE for the given subprogram and attach appropriate
  /// DW_AT_low_pc and DW_AT_high_pc attributes. If there are global
  /// variables in this scope then create and insert DIEs for these
  /// variables.
  DIE &updateSubprogramScopeDIE(DISubprogram SP);

  void constructScopeDIE(LexicalScope *Scope,
                         SmallVectorImpl<std::unique_ptr<DIE>> &FinalChildren);

  /// \brief A helper function to construct a RangeSpanList for a given
  /// lexical scope.
  void addScopeRangeList(DIE &ScopeDIE,
                         const SmallVectorImpl<InsnRange> &Range);

  void attachRangesOrLowHighPC(DIE &D,
                               const SmallVectorImpl<InsnRange> &Ranges);

  /// \brief This scope represents inlined body of a function. Construct
  /// DIE to represent this concrete inlined copy of the function.
  std::unique_ptr<DIE> constructInlinedScopeDIE(LexicalScope *Scope);

  /// \brief Construct new DW_TAG_lexical_block for this scope and
  /// attach DW_AT_low_pc/DW_AT_high_pc labels.
  std::unique_ptr<DIE> constructLexicalScopeDIE(LexicalScope *Scope);

  /// constructVariableDIE - Construct a DIE for the given DbgVariable.
  std::unique_ptr<DIE> constructVariableDIE(DbgVariable &DV,
                                            bool Abstract = false);

  std::unique_ptr<DIE> constructVariableDIE(DbgVariable &DV,
                                            const LexicalScope &Scope,
                                            DIE *&ObjectPointer);

  /// A helper function to create children of a Scope DIE.
  DIE *createScopeChildrenDIE(LexicalScope *Scope,
                              SmallVectorImpl<std::unique_ptr<DIE>> &Children,
                              unsigned *ChildScopeCount = nullptr);

  /// \brief Construct a DIE for this subprogram scope.
  void constructSubprogramScopeDIE(LexicalScope *Scope);

  DIE *createAndAddScopeChildren(LexicalScope *Scope, DIE &ScopeDIE);

  void constructAbstractSubprogramScopeDIE(LexicalScope *Scope);

  /// \brief Construct import_module DIE.
  std::unique_ptr<DIE>
  constructImportedEntityDIE(const DIImportedEntity &Module);

  void finishSubprogramDefinition(DISubprogram SP);

  void collectDeadVariables(DISubprogram SP);

  /// Set the skeleton unit associated with this unit.
  void setSkeleton(DwarfCompileUnit &Skel) { Skeleton = &Skel; }

  MCSymbol *getSectionSym() const {
    assert(Section);
    return SectionSym;
  }

  /// Pass in the SectionSym even though we could recreate it in every compile
  /// unit (type units will have actually distinct symbols once they're in
  /// comdat sections).
  void initSection(const MCSection *Section, MCSymbol *SectionSym) {
    DwarfUnit::initSection(Section);
    this->SectionSym = SectionSym;

    // Don't bother labeling the .dwo unit, as its offset isn't used.
    if (!Skeleton)
      LabelBegin =
          Asm->GetTempSymbol(Section->getLabelBeginName(), getUniqueID());
  }

  unsigned getLength() {
    return sizeof(uint32_t) + // Length field
        getHeaderSize() + UnitDie.getSize();
  }

  void emitHeader(const MCSymbol *ASectionSym) const override;

  MCSymbol *getLabelBegin() const {
    assert(Section);
    return LabelBegin;
  }

  /// Add a new global name to the compile unit.
  void addGlobalName(StringRef Name, DIE &Die, DIScope Context) override;

  /// Add a new global type to the compile unit.
  void addGlobalType(DIType Ty, const DIE &Die, DIScope Context) override;

  const StringMap<const DIE *> &getGlobalNames() const { return GlobalNames; }
  const StringMap<const DIE *> &getGlobalTypes() const { return GlobalTypes; }

  /// Add DW_AT_location attribute for a DbgVariable based on provided
  /// MachineLocation.
  void addVariableAddress(const DbgVariable &DV, DIE &Die,
                          MachineLocation Location);
  /// Add an address attribute to a die based on the location provided.
  void addAddress(DIE &Die, dwarf::Attribute Attribute,
                  const MachineLocation &Location, bool Indirect = false);

  /// Start with the address based on the location provided, and generate the
  /// DWARF information necessary to find the actual variable (navigating the
  /// extra location information encoded in the type) based on the starting
  /// location.  Add the DWARF information to the die.
  void addComplexAddress(const DbgVariable &DV, DIE &Die,
                         dwarf::Attribute Attribute,
                         const MachineLocation &Location);

  /// Add a Dwarf loclistptr attribute data and value.
  void addLocationList(DIE &Die, dwarf::Attribute Attribute, unsigned Index);
  void applyVariableAttributes(const DbgVariable &Var, DIE &VariableDie);

  /// Add a Dwarf expression attribute data and value.
  void addExpr(DIELoc &Die, dwarf::Form Form, const MCExpr *Expr);

  void applySubprogramAttributesToDefinition(DISubprogram SP, DIE &SPDie);
};

} // end llvm namespace

#endif
