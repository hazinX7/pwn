#include "module.h"

static noinline long ioctl_handler(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    
    switch (cmd) {
        case A_NEW_GAME: {
            struct new_game_request req;
            if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
                return -E_COPY_FROM_USER;
            }
            ret = create_new_game(req.player_name);
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                return -E_COPY_TO_USER;
            }
            break;
        }
        
        case A_MAKE_GUESS: {
            struct make_guess_request req;
            if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
                return -E_COPY_FROM_USER;
            }
            ret = make_guess(req.index, req.guess);
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                return -E_COPY_TO_USER;
            }
            break;
        }
        
        case A_GET_RESULT: {
            struct get_result_request req;
            if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
                return -E_COPY_FROM_USER;
            }
            ret = get_game_result(req.index, req.result);
            if (copy_to_user((void __user *)arg, &req, sizeof(req))) {
                return -E_COPY_TO_USER;
            }
            break;
        }
        
        case A_CHANGE_PLAYER_NAME: {
            struct change_player_name_request req;
            if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
                return -E_COPY_FROM_USER;
            }
            ret = change_player_name(req.index, req.new_player_name);
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                return -E_COPY_TO_USER;
            }
            break;
        }
        
        default:
            return -E_INCORRECT_REQ_OPTION;
    }
    
    return 0;
}


static noinline void cleanup_game(struct game *game)
{
    if (game->player_name) {
        kfree(game->player_name);
        game->player_name = NULL;
    }
    if (game->result) {
        kfree(game->result);
        game->result = NULL;
    }
    memset(game, 0, sizeof(struct game));
}

static noinline int grow_games_array(void)
{
    struct game *new_games;
    uint32_t new_capacity;
    
    new_capacity = games_capacity * GAMES_GROWTH_FACTOR;
    if (new_capacity == 0) {
        new_capacity = INITIAL_GAMES_CAPACITY;
    }
    
    new_games = krealloc(games, new_capacity * sizeof(struct game), GFP_KERNEL);
    if (!new_games) {
        return -ENOMEM;
    }
    
    memset(&new_games[games_capacity], 0, 
           (new_capacity - games_capacity) * sizeof(struct game));
    
    games = new_games;
    games_capacity = new_capacity;
    
    return 0;
}

static noinline int create_new_game(const char *player_name)
{
    struct game *new_game;
    uint32_t secret_number;
    int ret;
    
    mutex_lock(&games_mutex);
    
    if (games_count >= games_capacity) {
        ret = grow_games_array();
        if (ret) {
            mutex_unlock(&games_mutex);
            return ret;
        }
    }
    
    new_game = &games[games_count];
    
    new_game->player_name = kmalloc(MAX_PLAYER_NAME_LENGTH + 1, GFP_KERNEL);
    if (!new_game->player_name) {
        mutex_unlock(&games_mutex);
        return -E_KMALLOC_NULLPTR;
    }
    
    new_game->result = kmalloc(16, GFP_KERNEL);
    if (!new_game->result) {
        kfree(new_game->player_name);
        mutex_unlock(&games_mutex);
        return -E_KMALLOC_NULLPTR;
    }
    
    new_game->id = next_game_id++;
    memcpy(new_game->player_name, player_name, MAX_PLAYER_NAME_LENGTH);
    strcpy(new_game->result, "NE GOL");
    new_game->is_active = 1;
    
    get_random_bytes(&secret_number, sizeof(secret_number));
    new_game->secret_number = (secret_number % 100) + 1;
    
    games_count++;
    
    mutex_unlock(&games_mutex);
    
    return new_game->id;
}

static noinline int make_guess(uint64_t index, uint32_t guess)
{
    struct game *game;
    
    if (!games) {
        return -E_INVALID_GAME_ID;
    }
    
    mutex_lock(&games_mutex);
    
    game = &games[index];
    
    if (game->secret_number == guess) {
        strcpy(game->result, "GOOOOOL");
    } else {
        strcpy(game->result, "NE GOL");
    }
    
    mutex_unlock(&games_mutex);
    
    return 0;
}

static noinline int get_game_result(uint64_t index, char *result)
{
    struct game *game;
    
    if (!games) {
        return -E_INVALID_GAME_ID;
    }
    
    mutex_lock(&games_mutex);
    
    game = &games[index];
    
    memcpy(result, game->result, 16);
    
    mutex_unlock(&games_mutex);
    
    return 0;
}

static noinline int change_player_name(uint64_t index, const char *new_player_name)
{
    struct game *game;
    
    if (!games) {
        return -E_INVALID_GAME_ID;
    }
    
    mutex_lock(&games_mutex);
    
    game = &games[index];
    
    if (game->player_name) {
        memcpy(game->player_name, new_player_name, MAX_PLAYER_NAME_LENGTH);
    }
    
    mutex_unlock(&games_mutex);
    
    return 0;
}


static noinline int __init game_module_init(void)
{
    int ret;
    
    games = NULL;
    games_capacity = 0;
    games_count = 0;
    
    ret = misc_register(&kernel_gym_dev);
    if (ret) {
        printk(KERN_ERR "game: Failed to register misc device\n");
        return ret;
    }
    
    printk(KERN_INFO "game: Module loaded successfully\n");
    return 0;
}

static noinline void __exit game_module_exit(void)
{
    int i;
    
    if (games) {
        mutex_lock(&games_mutex);
        for (i = 0; i < games_count; i++) {
            cleanup_game(&games[i]);
        }
        mutex_unlock(&games_mutex);
        
        kfree(games);
        games = NULL;
        games_capacity = 0;
        games_count = 0;
    }
    
    misc_deregister(&kernel_gym_dev);
    
    printk(KERN_INFO "game: Module unloaded successfully\n");
}

module_init(game_module_init);
module_exit(game_module_exit);