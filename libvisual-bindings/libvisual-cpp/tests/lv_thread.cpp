// Libvisual-c++ - C++ bindings for Libvisual
//
// Copyright (C) 2005-2006 Chong Kai Xiong <descender@phreaker.net>
//
// Author: Chong Kai Xiong <descender@phreaker.net>
//
// $Id: lv_thread.cpp,v 1.3 2006-09-18 06:14:49 descender Exp $
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

#include <libvisual-cpp/lv_thread.hpp>
#include <iostream>
#include <queue>

template <class Lock>
class DebugLock
{
public:

    DebugLock ()
        : m_lock (new Lock),
          m_own (true)
    {}

    ~DebugLock ()
    {
        if (m_own)
            delete m_lock;
    }

    DebugLock (Lock& lock)
        : m_lock (&lock),
          m_own (false)
    {}

    void lock ()
    {
        std::cout << "DebugLock: locking." << std::endl;
        m_lock->lock ();
    }

    int try_lock ()
    {
        std::cout << "DebugLock: trying to lock." << std::endl;
        return m_lock->try_lock ();
    }

    void unlock ()
    {
        std::cout << "DebugLock: unlocking." << std::endl;
        m_lock->unlock ();
    }

private:

    Lock *m_lock;
    bool  m_own;
};

typedef DebugLock<Lv::Mutex> DebugMutex;

template <typename T, class Lock = Lv::Mutex>
class SynchronizedQueue
{
public:
    typedef Lv::ScopedLock<Lock> ScopedLock;

    SynchronizedQueue ()
    {}

    SynchronizedQueue (const SynchronizedQueue& other)
        : m_queue (other.m_queue)
    {}

    bool is_empty ()
    {
        ScopedLock lock(m_lock);

        return m_queue.empty ();
    }

    void enqueue (const T& item)
    {
        ScopedLock lock(m_lock);

        m_queue.push (item);
    }

    T dequeue ()
    {
        ScopedLock lock(m_lock);

        T item (m_queue.front ());
        m_queue.pop ();

        return item;
    }

private:

    std::queue<T> m_queue;
    Lock          m_lock;
};

template <class Queue>
class Consumer
{
public:

    Consumer (int id, Queue& queue, int limit)
        : m_id (id),
          m_queue (queue),
          m_limit (limit)
    {}

    void operator () ()
    {
        int item;
        int collected = 0;

        while (collected < m_limit)
        {
            if  (!m_queue.is_empty ())
            {
                item = m_queue.dequeue ();
                collected++;

                std::cout << m_id << ": Consumed: " << item << "\n";
            }
        }
    }

private:

    int    m_id;
    Queue& m_queue;
    int    m_limit;
};

template <class Queue>
class Producer
{
public:

    Producer (int id, Queue& queue, int limit)
        : m_id (id),
          m_queue (queue),
          m_limit (limit)
    {}

    void operator () ()
    {
        for (int i = 0; i < m_limit; i++)
        {
            m_queue.enqueue (i);

            std::cout << m_id << ": Produced: " << i << "\n";
            Lv::Thread::yield ();
        }

        std::cout << m_id << ": Finished production.\n";
    }

private:

    int    m_id;
    Queue& m_queue;
    int    m_limit;
};

void mutex_test ()
{
    std::cout << "Lv::Mutex test\n";

    Lv::Mutex actual_lock;
    DebugMutex lock (actual_lock);

    {
        Lv::ScopedLock<DebugMutex> scoped_lock(lock);
        std::cout << "Middle of scope 1\n";
    }

    {
        Lv::ScopedTryLock<DebugMutex> scoped_try_lock(lock);
        std::cout << "Middle of scope 2\n";
    }
}

void thread_test ()
{
    typedef SynchronizedQueue<int, Lv::Mutex> Queue;

    std::cout << "Lv::Thread test\n";

    Queue queue;

    // FIXME: gcc fails to compile the following lines for some
    // fscking reason, citing:
    //
    // sorry, unimplemented: inlining failed in call to $B!F(Bbool
    // boost::detail::function::has_empty_target(...)$B!G(B: function not
    // inlinable

    //Lv::Thread consumer  (Consumer<Queue> (0, queue, 50));
    //Lv::Thread producer1 (Producer<Queue> (1, queue, 25));
    //Lv::Thread producer2 (Producer<Queue> (2, queue, 25));

    //producer1.join ();
    //producer2.join ();
    //consumer.join ();
}

int main ()
{
    Lv::Thread::init ();

    mutex_test ();
    thread_test ();
}