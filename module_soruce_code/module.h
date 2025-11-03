#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h> 
#include <linux/random.h>
#include <linux/types.h>

MODULE_AUTHOR("ker@localhost");                    
MODULE_DESCRIPTION("Game");
MODULE_LICENSE("GPL");

typedef unsigned char uint8_t; 
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define MAX_PLAYER_NAME_LENGTH 64

// Game structure
struct game {
    uint32_t id;                    // Internal game ID
    char *player_name;              // Player name
    char *result;                   // Game result ("GOOOOOL" or "NE GOL")
    uint32_t secret_number;         // Secret number
    uint32_t is_active;             // Game activity flag
};

// IOCTL commands
static enum IOCTL_ACTIONS {
    A_NEW_GAME = 0x0101,           // Create new game
    A_MAKE_GUESS = 0x0102,         // Make guess
    A_GET_RESULT = 0x0103,         // Get result
    A_DELETE_GAME = 0x0104,        // Delete game
    A_CHANGE_PLAYER_NAME = 0x0105, // Change player name
};

// Structures for user requests
struct new_game_request {
    char player_name[MAX_PLAYER_NAME_LENGTH];
};

struct make_guess_request {
    uint64_t index;
    uint32_t guess;
};

struct get_result_request {
    uint64_t index;
    char result[16];
};

struct delete_game_request {
    uint64_t index;
};

struct change_player_name_request {
    uint64_t index;
    char new_player_name[MAX_PLAYER_NAME_LENGTH];
};

static enum ERORR_CODES {
    E_USERSPACE_POINTERS_INVL = 0x0e01,
    E_REQUEST_POINTER_IS_NULL = 0x0e02,
    E_INCORRECT_REQ_OPTION = 0x0e03,
    E_COPY_FROM_USER = 0x0e05,
    E_COPY_TO_USER = 0x0e0d,
    E_KMALLOC_NULLPTR = 0x0e11,
    E_USER_NULLPTR = 0x0e12,
    E_GAME_NOT_FOUND = 0x0e13,
    E_GAME_ALREADY_FINISHED = 0x0e14,
    E_INVALID_GAME_ID = 0x0e15,
};

// Global variables
#define INITIAL_GAMES_CAPACITY 10
#define GAMES_GROWTH_FACTOR 2
static struct game *games = NULL;  // Dynamic array on heap
static uint32_t games_capacity = 0;  // Current capacity of the array
static uint32_t games_count = 0;     // Current number of active games
static uint32_t next_game_id = 1;
static DEFINE_MUTEX(games_mutex);

// Function declarations
static noinline long ioctl_handler(struct file *, unsigned int, unsigned long);
static noinline int create_new_game(const char *player_name);
static noinline int make_guess(uint64_t index, uint32_t guess);
static noinline int get_game_result(uint64_t index, char *result);
static noinline int delete_game(uint64_t index);
static noinline int change_player_name(uint64_t index, const char *new_player_name);
static noinline void cleanup_game(struct game *game);
static noinline int grow_games_array(void);

static struct file_operations game_fops = {.unlocked_ioctl = ioctl_handler}; 

struct miscdevice kernel_gym_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "game",
    .fops = &game_fops,
};