# Game Kernel Module Vulnerability Analysis

The custom `/dev/game` driver exposes several ioctl actions that operate on an array of `struct game`. The implementation never validates the user controlled `index` argument before dereferencing, so every handler can read or write outside the allocated array.

Key vulnerable call sites:

* `make_guess()` fetches `game = &games[index];` and afterwards writes to `game->result` without any bounds check, enabling an arbitrary kernel write through the `result` pointer.【F:module_soruce_code/game.c†L147-L167】
* `get_game_result()` performs the same unchecked lookup and copies 16 bytes from the pointer stored at `game->result` back into userspace, giving a 16-byte arbitrary read primitive.【F:module_soruce_code/game.c†L170-L187】
* `change_player_name()` also dereferences `games[index]` without validation; if the struct at that address contains a controlled pointer value, the 64-byte memcpy becomes an arbitrary write primitive.【F:module_soruce_code/game.c†L189-L208】

## Exploitation strategy

1. **Leak the heap base of the `games` array**. The pointer to the dynamically allocated array is stored in the module BSS (`games` symbol). Using the out-of-bounds read in `get_game_result` with an index that maps to the BSS (computed from `/sys/module/game/sections/.bss` and the leaked `games` pointer) returns 16 bytes starting at `games`, revealing the heap pointer along with the first `struct game` metadata.
2. **Build arbitrary read/write primitives.** Once the heap base is known, the unchecked index lets us position `game` over any kernel address. The `get_game_result` ioctl becomes a 16-byte arbitrary read and `change_player_name` a 64-byte arbitrary write by crafting fake `struct game` instances in controlled memory.
3. **Escalate privileges.** Overwrite `modprobe_path` with a path to a userspace helper, drop a crafted binary to `/tmp`, and trigger `modprobe` (for example via an invalid ELF file) to execute arbitrary commands as root and read `/flag`.

## Remote access note

The publicly provided deployer endpoint `nc 46.243.172.105 17002` is currently unreachable from the execution environment that produced this write-up (`connect()` fails with `Network is unreachable`). Any end-to-end exploitation attempt therefore has to be performed either locally (e.g. by running `service/run.sh` with QEMU) or from an environment with unrestricted outbound connectivity to the deployer.

Because SMEP/SMAP are enabled, the exploit keeps execution in kernel space by only overwriting kernel data (`modprobe_path`) instead of redirecting control flow.
