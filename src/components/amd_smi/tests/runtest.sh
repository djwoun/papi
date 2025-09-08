#!/usr/bin/env bash
# AMD SMI test runner (no build) with print control

# Be reasonably strict but compatible with older bash
set -e
set -u
( set -o pipefail ) 2>/dev/null || true

# Try to infer AMD SMI root if not set
: "${PAPI_AMDSMI_ROOT:=${PAPI_ROCM_ROOT:-/opt/rocm-6.4.0}}"

banner() { printf "Running: \033[36m%s\033[0m %s\n" "$1" "${2-}"; }
sep()    { printf "%s\n\n" "-------------------------------------"; }

# ---------------------------
# CLI: --print | --print-only
# ---------------------------
PRINT_ALL=0
PRINT_SET=""     # comma list: hello,basics,gemm,energy,conflict
HELLO_EVENT="amd_smi:::temp_current:device=0:sensor=1"

usage() {
  cat <<EOF
Usage: $0 [--print] [--print-only=list] [--hello-event=EVENT]

  --print                 Enable verbose (--print) for all tests
  --print-only=list       Comma-separated subset: hello,basics,gemm,energy,conflict
  --hello-event=EVENT     Override event passed to amdsmi_hello
EOF
}

for arg in "$@"; do
  case "$arg" in
    TESTS_QUIET)   ;;               # ignore, invoked by src/run_tests.sh
    --print)       PRINT_ALL=1 ;;
    --print-only=*) PRINT_SET="${arg#*=}" ;;
    --hello-event=*) HELLO_EVENT="${arg#*=}" ;;
    -h|--help)     usage; exit 0 ;;
    *) echo "Unknown option: $arg"; usage; exit 2 ;;
  esac
done


in_set() {
  # $1 = token, $2 = comma list
  case ",$2," in
    *,"$1",*) return 0 ;;
    *) return 1 ;;
  esac
}

should_print() {
  # $1 = test key (hello|basics|gemm|energy|conflict)
  if [ "$PRINT_ALL" -eq 1 ]; then return 0; fi
  if [ -n "$PRINT_SET" ] && in_set "$1" "$PRINT_SET"; then return 0; fi
  return 1
}

run_test() {
  # $1 = binary, $2 = key, $3.. = extra args (e.g., default event)
  local bin="$1"; shift
  local key="$1"; shift || true

  if [[ ! -x "./$bin" ]]; then
    echo "SKIP: missing ./$bin"; sep; return 0
  fi

  if should_print "$key"; then
    # Per-invocation verbose: unset quiet and pass --print
    banner "./$bin --print" "$*"
    ( unset PAPI_AMDSMI_TEST_QUIET; "./$bin" --print "$@" ) || true
  else
    # Per-invocation quiet: set quiet and do not pass --print
    banner "./$bin" "$*"
    PAPI_AMDSMI_TEST_QUIET=1 "./$bin" "$@" || true
  fi
  sep
}

# ---------------------------
# Run the five tests
# ---------------------------

run_test amdsmi_hello         hello    "$HELLO_EVENT"
run_test amdsmi_basics        basics
run_test amdsmi_gemm          gemm
run_test amdsmi_energy_monotonic energy
run_test amdsmi_ctx_conflict  conflict

# Done
unset PAPI_AMDSMI_TEST_QUIET
