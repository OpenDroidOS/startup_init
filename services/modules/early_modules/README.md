# Ramdisk kernel modules

A product whose root storage needs kernel modules can select
`//base/startup/init/services/modules/early_modules:early_modules` through the
existing `init_extra_static_modules` component feature. If the product already
supplies a static extension, compose both targets in a group. This module does
not change the feature's existing product semantics or enable itself by default.

Place a nonempty ordered `modules.load.ohos` and its named module files together
in `/lib/modules` in each native ramdisk. The distinct filename preserves native
selection when a bootloader appends vendor ramdisks containing `modules.load`.
For existing products, `modules.load` is read only when `modules.load.ohos` is
absent. An invalid or unreadable native list is an error, not a fallback request.
The list accepts one flat `.ko` filename per
line, blank lines and full-line `#` comments, with at most 512 modules and 128 KiB
of text. Paths, parameters, duplicate names, symlinks and nonregular files are
rejected. The complete list is checked before any kernel load is attempted.
Select and validate the full dependency order and kernel/DT/firmware contracts
outside init. The loader does not infer binary compatibility or dependencies.

The static hook runs at `INIT_FIRST_STAGE`, after filesystem/device setup and
before required-device discovery and mounts. It uses `finit_module` with an
empty parameter string and zero flags, preserving the kernel's normal checks.
A kernel `EEXIST` result is accepted when a preceding init stage has already
loaded the module. The loader still opens and submits each selected file; it
does not skip files based on names in userspace. Other kernel errors remain
fatal. This does not establish that previously loaded module bytes match the
selected files; products must validate the complete preceding-stage inputs.
A required module or list failure is logged and stops early init. A missing or
empty list is an error for products that explicitly select this module.

The module does not create logical mappings, supply a required fstab or assemble
boot containers. These inputs must also be configured by the product.
