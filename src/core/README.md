# core

This directory is the long-term home for virtualization core modules.

Current migration status:

- `core/vmm` has been promoted as the first migrated module group.
- Remaining `device`, `arch`, and `net` modules are still compiled from legacy paths
  and will be moved here incrementally to keep refactors safe.

The current layout intentionally keeps compatibility includes in place while the
rest of the tree is migrated.
