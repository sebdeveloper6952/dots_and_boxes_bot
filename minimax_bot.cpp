#include <functional>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>
#include "./socket.io-client-cpp/build/include/sio_client.h"

using namespace sio;
using namespace std;

#define INF 99999999
#define USERNAME "SebasArriola"
#define TOURNAMENT_ID 999999
#define FULL 0
#define EMPTY 99
// #define URL "http://localhost:4000"
#define URL "http://3.12.129.126:4000"
// #define NGROK "http://876e10de0c24.ngrok.io/"
#define HALF_A_SEC 500000
#define O_DEPTH 4

std::mutex _lock;
std::condition_variable_any _cond;
bool connect_finish = false;
struct minimax_result {int score; int i; int j;};
socket::ptr _socket;
long curr_game_id = 0;
int depth = O_DEPTH;

void print_board(int board[][30])
{
    for (int i = 0; i < 30; i++)
        std::cout << board[0][i] << ",";
    std::cout << std::endl;
    for (int i = 0; i < 30; i++)
        std::cout << board[1][i] << ",";
    std::cout << std::endl;
}

void parse_board(std::vector<message::ptr> board, int board_a[][30])
{
    std::vector<message::ptr> h = board[0]->get_vector();
    std::vector<message::ptr> v = board[1]->get_vector();
    for (int i = 0; i < h.size(); i++)
    {
        board_a[0][i] = h[i]->get_int();
        board_a[1][i] = v[i]->get_int();
    }
}

void copy_board(int board[][30], int new_board[][30])
{
    for (int i = 0; i < 30; i++)
    {
        new_board[0][i] = board[0][i];
        new_board[1][i] = board[1][i];
    }
}

int count_squares(int board[][30])
{
    int square_count = 0, size = 6, a = 0, c = 0;
    for (int i = 0; i < 30; i++)
    {
        if (((i + 1) % size) != 0)
        {
            if (board[0][i] != 99 && board[0][(i + 1)] != 99 &&
                board[1][(c + a)] != 99 && board[1][(c + a + 1)] != 99)
            {
                square_count++;
            }
            a += size;
        }
        else
        {
            c++;
            a = 0;
        }
    }
    return square_count;
}

int new_squares_created(int board[][30], int i, int j)
{
    int a = 0, b = count_squares(board);
    board[i][j] = FULL;
    a = count_squares(board);
    board[i][j] = EMPTY;
    return (a - b);
}

bool game_over(int board[][30])
{
    for (int i = 0; i < 30; i++)
        if (board[0][i] == EMPTY || board[1][i] == EMPTY)
            return false;
    return true;
}

void calculate_scores(int board[][30], int *p1, int *p2)
{
    for (int i = 0; i < 30; i++)
    {
        int h = board[0][i];
        int v = board[1][i];
        if (h != 99)
        {
            if (h > 0) *p1 += h;
            else if (h < 0) *p2 += h;
        }
        if (v != 99)
        {
            if (v > 0) *p1 += v;
            else if (v < 0) *p2 += v;
        }
    }
}

int moves_remaining(int board[][30])
{
    int r = 0;
    for (int i = 0; i < 30; i++)
    {
        if (board[0][i] == EMPTY) r++;
        if (board[1][i] == EMPTY) r++;
    }
    return r;
}

struct minimax_result static_evaluation(int board[][30])
{
    int p1 = 0, p2 = 0;
    struct minimax_result res;
    res.i = 0;
    res.j = 0;
    calculate_scores(board, &p1, &p2);
    res.score = p1 + p2;
    return res;
}

struct minimax_result minimax(int board[][30], int depth, bool is_max, int alpha, int beta)
{
    if (depth == 0)
    {
        struct minimax_result res = static_evaluation(board);
        return res;
    }
    else if (game_over(board))
    {
        struct minimax_result res = static_evaluation(board);
        return res;
    }

    if (is_max)
    {
        struct minimax_result best_res;
        best_res.score = -INF;
        best_res.i = 0;
        best_res.j = 0;
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 30; j++)
            {
                if (board[i][j] == EMPTY)
                {
                    int sq = new_squares_created(board, i, j);
                    int new_board[2][30];
                    copy_board(board, new_board);
                    new_board[i][j] = sq;
                    struct minimax_result res;
                    res = minimax(new_board, depth - 1, sq > 0, alpha, beta);
                    if (res.score > best_res.score)
                    {
                        best_res.score = res.score;
                        best_res.i = i;
                        best_res.j = j;
                    }
                    alpha = alpha > res.score ? alpha : res.score;
                    if (beta <= alpha) break;
                }
                if (beta <= alpha) break;
            }
        }
        return best_res;
    }
    else
    {
        struct minimax_result best_res;
        best_res.score = INF;
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 30; j++)
            {
                if (board[i][j] == EMPTY)
                {
                    int sq = new_squares_created(board, i, j);
                    int new_board[2][30];
                    copy_board(board, new_board);
                    new_board[i][j] = sq > 0 ? -sq : 0;
                    struct minimax_result res;
                    res = minimax(new_board, depth - 1, !(sq > 0), alpha, beta);
                    if (res.score < best_res.score)
                    {
                        best_res.score = res.score;
                        best_res.i = i;
                        best_res.j = j;
                    }
                    beta = beta < res.score ? beta : res.score;
                    if (beta <= alpha) break;
                }
                if (beta <= alpha) break;
            }
        }
        return best_res;
    }
}

void on_connect()
{
    std::cout<< "Connected to server!" <<std::endl;
    _lock.lock();
    _cond.notify_all();
    connect_finish = true;
    _lock.unlock();
}

void on_ok_signin(sio::event &ev)
{
    std::cout<< "Sign in successful!" <<std::endl;
}

void on_ready(sio::event &ev)
{
    message::ptr msg = ev.get_message();
    std::map<std::string, message::ptr> data = msg->get_map();
    curr_game_id = data["game_id"]->get_int();
    int player_turn_id = data["player_turn_id"]->get_int();
    std::vector<message::ptr> board = data["board"]->get_vector();

    int board_a[2][30];
    parse_board(board, board_a);

    // minimax with board
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    struct minimax_result res;
    res = minimax(board_a, depth, true, -INF, INF);
    // timing and lookahead increase
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    long diff = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    
    // debug data
    std::cout << "**********************************************" << std::endl;
    std::cout << "score: " << res.score << std::endl;
    std::cout << "playin move: " << res.i << "," << res.j << std::endl;
    std::cout << "depth: " << depth << std::endl;
    std::cout << "time diff: " << diff << "[µs]" << std::endl;
    std::cout << "**********************************************" << std::endl;
    std::cout << std::endl;
    

    if (moves_remaining(board_a) < 50 && diff < 250000)
    {
        depth++;
        std::cout << "Depth increased to: " << depth << std::endl;
    }

    // emit play
    message::ptr emit_data = object_message::create();
    emit_data->get_map()["game_id"] = int_message::create(curr_game_id);
    emit_data->get_map()["tournament_id"] = int_message::create(TOURNAMENT_ID);
    emit_data->get_map()["player_turn_id"] = int_message::create(player_turn_id);

    message::ptr move = array_message::create();
    move->get_vector().push_back(int_message::create(res.i));
    move->get_vector().push_back(int_message::create(res.j));
    emit_data->get_map()["movement"] = move;
    _socket->emit("play", emit_data);
}

void on_finish(sio::event &ev)
{
    // reset depth
    depth = O_DEPTH;

    // get data
    std::map<std::string, message::ptr> data = ev.get_message()->get_map();
    std::vector<message::ptr> board = data["board"]->get_vector();
    int player_turn_id = data["player_turn_id"]->get_int();
    int winner_turn_id = data["winner_turn_id"]->get_int();
    int board_a[2][30];
    int p1 = 0, p2 = 0;
    parse_board(board, board_a);
    calculate_scores(board_a, &p1, &p2);
    std::cout << "**************** GAME OVER *******************" << std::endl;
    std::cout << "* Did i win? " << (player_turn_id == winner_turn_id ? "YES!":"NO :(") << std::endl;
    std::cout << "* Player 1: " << p1 << std::endl;
    std::cout << "* Player 2: " << (-p2) << std::endl;
    
    // emit player_ready
    message::ptr emit_data = object_message::create();
    emit_data->get_map()["tournament_id"] = int_message::create(TOURNAMENT_ID);
    emit_data->get_map()["player_turn_id"] = int_message::create(player_turn_id);
    emit_data->get_map()["game_id"] = int_message::create(curr_game_id);
    _socket->emit("player_ready", emit_data);
}

int main(int argc, const char *args[])
{

    sio::client h;
    
    h.set_open_listener(&on_connect);
    h.connect(URL);
    
    _lock.lock();
    if(!connect_finish)
    {
        _cond.wait(_lock);
    }
    _lock.unlock();

    // save global socket
    _socket = h.socket();

    // bind all events
    _socket->on("ok_signin", &on_ok_signin);
    _socket->on("ready", &on_ready);
    _socket->on("finish", &on_finish);

    // sign in
    message::ptr data = object_message::create();
    data->get_map()["user_name"] = string_message::create(USERNAME);
    data->get_map()["tournament_id"] = int_message::create(TOURNAMENT_ID);
    data->get_map()["user_role"] = string_message::create("player");
    _socket->emit("signin", data);

    while(1) {}
    
    // h.sync_close();
    // h.clear_con_listeners();
	return 0;
}
