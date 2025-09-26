# AMD_SMI Component

The **AMD_SMI** (AMD System Management Interface) component exposes hardware
management counters (and selected controls) for AMD GPUs — e.g., power usage,
temperatures, clocks, PCIe link metrics, VRAM information, and RAS/ECC status —
by querying the AMD SMI library at runtime (ROCm ≥ 6.4.0).

- [Environment Variables](#environment-variables)
- [Enabling the AMD_SMI Component](#enabling-the-amd_smi-component)

---

## Environment Variables

For AMD_SMI, PAPI requires the environment variable `PAPI_AMDSMI_ROOT` to be set
so that the AMD SMI shared library and headers can be found. This variable is
required at both **compile** and **run** time.

**Setting PAPI_AMDSMI_ROOT**  
Set `PAPI_AMDSMI_ROOT` to the top-level ROCm directory. For example:

   ```bash
   export PAPI_AMDSMI_ROOT=/opt/rocm-6.4.0
   # or
   export PAPI_AMDSMI_ROOT=/opt/rocm
   ```

The directory specified by `PAPI_AMDSMI_ROOT` **must contain** the following
subdirectories:

- `PAPI_AMDSMI_ROOT/lib` (which should include the dynamic library `libamd_smi.so`)
- `PAPI_AMDSMI_ROOT/include/amd_smi` (AMD SMI headers)

If the library is not found or is not functional at runtime, the component will
appear as "disabled" in `papi_component_avail`, with a message describing the
problem (e.g., library not found).

---

## Enabling the AMD_SMI Component

To enable reading (and where supported, writing) of AMD_SMI counters, build
PAPI with this component enabled. For example:

```bash
./configure --with-components="amd_smi"
make
```

You can verify availability with the utilities in `papi/src/utils/`:

```bash
papi_component_avail            # shows enabled/disabled components
papi_native_avail -i amd_smi    # lists native events for this component
```

---

