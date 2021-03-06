// The MIT License (MIT)

// Copyright (c) 2016, Microsoft

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include "gtest/gtest.h"

#include "PackedArray.h"


namespace BitFunnel
{
    namespace PackedArrayTest
    {
        void RunTest1(unsigned capacity, bool useVirtualAlloc);


        TEST(PackedArray, HeapAlloc)
        {
            RunTest1(1, false);
            RunTest1(2, false);

            // Try one run with mmap/VirtualAlloc().
            RunTest1(3, true);
        }


        TEST(PackedArray, VirtualAlloc)
        {
            RunTest1(4, true);
        }


        TEST(PackedArray, RoundTrip)
        {
            // First test that array round trips correctly without virtual alloc.
            unsigned capacity = 100;
            unsigned bitsPerEntry = 9;
            bool useVirtualAlloc = false;

            PackedArray a(capacity, bitsPerEntry, useVirtualAlloc);
            for (unsigned i = 0 ; i < capacity; ++i)
            {
                a.Set(i, i %512);
            }

            std::stringstream stream;
            a.Write(stream);

            PackedArray b(stream);

            EXPECT_EQ(a.GetCapacity(), b.GetCapacity());
            for (unsigned i = 0 ; i < capacity; ++i)
            {
                EXPECT_EQ(b.Get(i), i % 512);
            }

            // TODO: Test that the virtual alloc flag round trips.
        }


        void RunTest1(unsigned capacity, bool useVirtualAlloc)
        {
            for (unsigned bitsPerEntry = 1; bitsPerEntry < 57; ++bitsPerEntry)
            {
                PackedArray packed(capacity, bitsPerEntry, useVirtualAlloc);

                uint64_t modulus = 1ULL << bitsPerEntry;

                for (unsigned i = 0 ; i < capacity; ++i)
                {
                    packed.Set(i, i % modulus);

                    // Verify contents of buffer.
                    for (unsigned j = 0 ; j < capacity; ++j)
                    {
                        uint64_t expected = (j <= i) ? (j % modulus) : 0;
                        uint64_t found = packed.Get(j);

                        // If we have written the index, we expect the value we wrote.
                        EXPECT_EQ(expected, found);
                    }
                }
            }
        }
    }
}
