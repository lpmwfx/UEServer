# Project Documentation Structure - HOWTO

This document explains the documentation files used in this project and their purposes.

## Core Documentation Files

### TODO
**Purpose:** Current phase task tracking
**Scope:** One phase at a time
**When to update:**
- At start of new phase: Clear and write new phase tasks
- During work: Check off completed items
- When phase complete: Archive to DONE

**Example:** Phase 2 tasks for MCP Integration

---

### DONE (in `doc/DONE`)
**Purpose:** Archive of completed work
**Scope:** All finished phases
**When to update:**
- When a phase is 100% complete
- Move finished phase from TODO to DONE
- Include test results and deliverables

**Example:** Phase 1 RPC Bridge MVP (completed 2025-12-26)

---

### RAG
**Purpose:** Research findings and manual references
**Scope:** Links, searches, and documentation references
**When to update:**
- When finding relevant documentation
- When searching web/manuals for solutions
- When discovering important file locations

**Example:** Link to UE5 plugin documentation, BlenderServer reference

---

### CHANGELOG
**Purpose:** Point of truth for all code changes
**Scope:** Every implementation and modification
**When to update:**
- After implementing features
- After fixing bugs
- After testing (include test results)
- At end of session (summary)

**Example:** "Added ue.health tool with 500ms timeout"

---

### Project-*.md
**Purpose:** High-level project description
**Scope:** Architecture, goals, technology stack
**When to update:**
- At project start (initial design)
- When architecture changes
- Rarely (mostly read-only reference)

**Example:** `CLAUDE.md` describes UEServer architecture

---

## Workflow Example

### Starting New Phase
1. Read `DONE` to see what's finished
2. Read `TODO` for current phase plan
3. Work on tasks, checking them off
4. Update `RAG` with research findings
5. Update `CHANGELOG` with implementations

### Completing Phase
1. Mark all TODO items complete
2. Test everything thoroughly
3. Move phase from `TODO` to `DONE` with results
4. Create new `TODO` for next phase
5. Update `CHANGELOG` with final summary
6. Update `RAG` with final links/findings

---

## Quick Reference

| File | Content | Update Frequency |
|------|---------|------------------|
| TODO | Current phase tasks | Daily / Per task |
| DONE | Completed phases | Per phase |
| RAG | Research & links | As needed |
| CHANGELOG | All code changes | Per implementation |
| Project-*.md | Project overview | Rarely |

---

## File Locations

```
/home/lpm/REPO/UEServer/
├── TODO              # Current phase tasks
├── CHANGELOG         # All changes
├── RAG               # Research findings
├── doc/
│   ├── DONE          # Completed work archive
│   └── project-HOWTO.md  # This file
└── CLAUDE.md         # Project architecture (checked into repo)
```

---

**Remember:**
- TODO = What to do NOW
- DONE = What's been finished
- RAG = What we found/learned
- CHANGELOG = What changed in code
