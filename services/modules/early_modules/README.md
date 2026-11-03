# Ramdisk kernel modules

A product whose root storage needs kernel modules can select
`//base/startup/init/services/modules/early_modules:early_modules` through the
existing `init_extra_static_modules` component feature. If the product already
supplies a static extension, compose both targets in a group. This module does
not change the feature's existing product semantics or enable itself by default.

Place a nonempty ordered `modules.load` and its named module files together in
`/lib/modules` in the boot ramdisk. The list accepts one flat `.ko` filename per
line, blank lines and full-line `#` comments, with at most 512 modules and 128 KiB
of text. Paths, parameters, duplicate names, symlinks and nonregular files are
rejected. The complete list is checked before any kernel load is attempted.
Select and validate the full dependency order and kernel/DT/firmware contracts
outside init. The loader does not infer binary compatibility or dependencies.

The static hook runs at `INIT_FIRST_STAGE`, after filesystem/device setup and
before required-device discovery and mounts. It uses `finit_module` with an
empty parameter string and zero flags, preserving the kernel's normal checks.
A required module or list failure is logged and stops early init. A missing or
empty list is an error for products that explicitly select this module.

The module does not create logical mappings, supply a required fstab or assemble
boot containers. These inputs must also be configured by the product.
