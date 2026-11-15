#include <vector>
#include <memory>
#include <random>
#include <coroutine>
#include <stdexcept>
#include <algorithm>
#include <print>

using namespace std;

struct TurnRecord
{
  int player_id;
  int guess;
  int reply;
};

class Host
{
public:
  struct promise_type;
  using handle = coroutine_handle<promise_type>;

  int ask(int question)
  {
    if (!coro || coro.done())
    {
      throw runtime_error("host coroutine is inactive");
    }
    *coro.promise().question = question;
    coro.resume();
    return coro.promise().answer;
  }

  struct promise_type
  {
    int answer = 0;
    shared_ptr<int> question;

    promise_type(shared_ptr<int> q, int, int) : question(q) {}

    void unhandled_exception()
    {
      terminate();
    }

    auto initial_suspend()
    {
      return suspend_always{};
    }

    auto final_suspend() noexcept
    {
      return suspend_always{};
    }

    auto yield_value(int value)
    {
      answer = value;
      return suspend_always{};
    }

    auto get_return_object()
    {
      return Host{handle::from_promise(*this)};
    }

    void return_void() {}
  };

  Host(Host const&) = delete;
  Host& operator=(Host const&) = delete;

  Host(Host&& other) noexcept : coro(other.coro)
  {
    other.coro = nullptr;
  }

  Host& operator=(Host&& other) noexcept
  {
    if (this != &other)
    {
      if (coro)
      {
        coro.destroy();
      }
      coro = other.coro;
      other.coro = nullptr;
    }
    return *this;
  }

  ~Host()
  {
    if (coro)
    {
      coro.destroy();
    }
  }

private:
  explicit Host(handle h) : coro(h) {}
  handle coro;
};

class Player
{
public:
  struct promise_type;
  using handle = coroutine_handle<promise_type>;

  int next_guess()
  {
    if (!coro || coro.done())
    {
      throw runtime_error("player coroutine is inactive");
    }
    coro.resume();
    return coro.promise().guess;
  }

  struct promise_type
  {
    int guess = 0;

    void unhandled_exception()
    {
      terminate();
    }

    auto initial_suspend()
    {
      return suspend_always{};
    }

    auto final_suspend() noexcept
    {
      return suspend_always{};
    }

    auto yield_value(int value)
    {
      guess = value;
      return suspend_always{};
    }

    auto get_return_object()
    {
      return Player{handle::from_promise(*this)};
    }

    void return_void() {}
  };

  Player(Player const&) = delete;
  Player& operator=(Player const&) = delete;

  Player(Player&& other) noexcept : coro(other.coro)
  {
    other.coro = nullptr;
  }

  Player& operator=(Player&& other) noexcept
  {
    if (this != &other)
    {
      if (coro)
      {
        coro.destroy();
      }
      coro = other.coro;
      other.coro = nullptr;
    }
    return *this;
  }

  ~Player()
  {
    if (coro)
    {
      coro.destroy();
    }
  }

private:
  explicit Player(handle h) : coro(h) {}
  handle coro;
};

Host make_host(shared_ptr<int> question, int min_val, int max_val)
{
  if (min_val > max_val)
  {
    throw invalid_argument("invalid range of host");
  }

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<int> dist(min_val, max_val);
  int secret = dist(gen);

  while (true)
  {
    int q = *question;
    int reply = 0;
    if (q < secret)
    {
      reply = -1;
    }
    else if (q > secret)
    {
      reply = 1;
    }
    co_yield reply;
  }
}

Player make_player(shared_ptr<vector<TurnRecord>> history, int min_val, int max_val)
{
  if (min_val > max_val)
  {
    throw invalid_argument("invalid range of player");
  }

  int low = min_val;
  int high = max_val;
  size_t seen = 0;

  random_device rd;
  mt19937 gen(rd());

  while (true)
  {
    while (seen < history->size())
    {
      const auto& rec = (*history)[seen];
      if (rec.reply == -1)
      {
        low = max(low, rec.guess + 1);
      }
      else if (rec.reply == 1)
      {
        high = min(high, rec.guess - 1);
      }
      ++seen;
    }

    uniform_int_distribution<int> dist(low, high);
    int guess = dist(gen);
    co_yield guess;
  }
}

string reply_description(int reply)
{
  if (reply == -1)
  {
    return "-1 ";
  }
  if (reply == 1)
  {
    return " 1 ";
  }
  return " 0 ";
}

void run_game(int min_val = 1, int max_val = 100)
{
  if (min_val > max_val)
  {
    throw invalid_argument("помилка: мін значення більше за макс");
  }

  auto question = make_shared<int>(0);
  auto history = make_shared<vector<TurnRecord>>();

  Host host = make_host(question, min_val, max_val);
  Player player1 = make_player(history, min_val, max_val);
  Player player2 = make_player(history, min_val, max_val);

  println("Гра «вгадай число» (діапазон [{}, {}])\n", min_val, max_val);

  int step = 1;
  int winner = 0;

  while (winner == 0)
  {
    int current_player = (step % 2 != 0) ? 1 : 2;
    Player& active_player = (current_player == 1) ? player1 : player2;

    int guess = active_player.next_guess();
    int reply = host.ask(guess);

    history->push_back({current_player, guess, reply});

    println("крок {:2d} | гравець {} називає: {:3d} | відповідь ведучого: {}",
            step, current_player, guess, reply_description(reply));

    if (reply == 0)
    {
      winner = current_player;
    }
    else
    {
      ++step;
    }
  }

  println("\nГру завершено на кроці {}! Переможець: гравець {}, вгадав число {}",
          step, winner, history->back().guess);
}

int main()
{
  try
  {
    run_game(1, 100);
  }
  catch (const exception& e)
  {
    println(stderr, "Помилка: {}", e.what());
    return 1;
  }

  return 0;
}
