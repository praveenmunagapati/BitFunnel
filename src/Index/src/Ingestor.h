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

#pragma once

#include <atomic>                           // std::atomic member.
#include <memory>                           // std::unique_ptr embedded.
#include <mutex>                            // std::mutex member.
#include <stddef.h>                         // size_t template parameter.
#include <vector>                           // std::vector embedded.

#include "BitFunnel/BitFunnelTypes.h"       // DocId parameter.
#include "BitFunnel/Index/IIngestor.h"      // Inherits from IIngestor.
#include "BitFunnel/Index/Token.h"          // ITokenManager parameterizes std::unique_ptr.
#include "BitFunnel/NonCopyable.h"          // Base class.
#include "DocumentCache.h"                  // DocumentCache embedded.
#include "DocumentHistogramBuilder.h"       // Embeds DocumentHistogramBuilder.
#include "DocumentMap.h"                    // DocumentMap template parameter.
#include "Shard.h"                          // std::unique_ptr template parameter.


namespace BitFunnel
{
    class IDocumentDataSchema;
    class IShardDefinition;
    class ISliceBufferAllocator;
    class ITermTableCollection;


    class Ingestor : public IIngestor, NonCopyable
    {
    public:
        Ingestor(IDocumentDataSchema const & docDataSchema,
                 IRecycler& recycle,
                 ITermTableCollection const & termTables,
                 IShardDefinition const & shardDefinition,
                 ISliceBufferAllocator& sliceBufferAllocator);

        virtual ~Ingestor();

        // TODO: Remove this temporary method.
        virtual void PrintStatistics(std::ostream& out,
                                     double time) const override;

        // Writes out the following data structions in locations defined by the
        // FileManager:
        //
        //   Per IIngester
        //      DocumentHistogramBuilder
        //   Per Shard
        //      CumulativeTermCountd
        //      DocumentFrequencyTable (with term text if termToText provided)
        //      IndexedIdfTable
        virtual void WriteStatistics(IFileManager & fileManager,
                                     ITermToText const * termToText) const override;

        virtual void TemporaryWriteAllSlices(IFileManager& fileManager) const override;

        // Returns a reference to the IDocument cache. This cache holds ingested
        // IDocuments for use in query verification diagnostics.
        virtual IDocumentCache & GetDocumentCache() const override;


        // Adds a document to the index. Throws if there is no space to add the
        // document which means the system is running at its maximum capacity.
        // The IDocument must implement the Place method which should call
        // IIndex::AllocateDocument, passing the ddrCandidateCount parameter.
        // Throws if the index already contains a document with the same id
        // value.
        virtual void Add(DocId id, IDocument const & document) override;

        // Removes a document from serving. The document with the specified id
        // will no longer be returned from the queries. Returns true if the
        // document was successfully removed and false otherwise. False means
        // that a document with this id wasn't added previously or it was
        // already removed.
        //
        // DESIGN NOTE: The system supports deleting a document that has been
        // already deleted in order to simplify bulk deletion of documents,
        // some of which may already have been deleted for other reasons.
        virtual bool Delete(DocId id) override;

        // Sets or clears a fact about a document with the given DocId. The
        // FactHandle must have been previously registered in the IFactSet,
        // otherwise the function throws.
        // TODO: Update this comment to explain which IFactSet is referred
        // to above.
        virtual void AssertFact(DocId id, FactHandle fact, bool value) override;

        // Returns true if and only if the specified DocId corresponds to a
        // document currently visible to the query processing system. Returns
        // false for DocIds that have never been added, DocIds that are
        // partially ingested, and DocIds that have been deleted.
        virtual bool Contains(DocId id) const override;

        virtual DocumentHandle GetHandle(DocId id) const override;

        // Returns the number of documents currently active in the index.
        virtual size_t GetDocumentCount() const override;

        // Returns the size in bytes of the capacity of row tables in the
        // entire ingestion index.
        virtual size_t GetUsedCapacityInBytes() const override;

        // Returns the total number of bytes in the source representation of
        // all IDocuments ingested so far.
        virtual size_t GetTotalSouceBytesIngested() const override;

        // Returns the total number of postings in all documents ingested.
        // This count is not reduced as documents are deleted.
        virtual size_t GetPostingCount() const override;

        // Returns a number of Shards and a Shard with the given ShardId.
        virtual size_t GetShardCount() const override;
        virtual IShard& GetShard(size_t shard) const override;

        virtual IRecycler& GetRecycler() const override;

        virtual ITokenManager& GetTokenManager() const override;

        // Shuts down the index and releases resources allocated to it.
        virtual void Shutdown() override;

        //
        // Group management functions.
        //
        // A group is a sequence of documents that were ingested after the opening
        // of the group and its sealing. Once the group is closed, it is considered
        // immutable. When expiring a group, all the data related to documents
        // that were part of this group will be deleted.
        //

        // Opens a new group and assigns it the given group id.
        //    - All future addition operations are done in this new group.
        //    - The previous group is closed. A closed group cannot be reopened or
        //      modified.
        virtual void OpenGroup(GroupId groupId) override;

        // Closes the current group, if any.
        virtual void CloseGroup() override;

        // Expires the group with the given id.
        virtual void ExpireGroup(GroupId groupId) override;

    private:
        IRecycler& m_recycler;
        IShardDefinition const & m_shardDefinition;

        // TODO: Replace these tempoary statistics variables with document
        // length hash table and term frequency tables.
        // TODO: This member is now redundant (with DocumentMap).
        // Note that documentCount will not always be equal to
        // the size of the unordered_map in m_documentMap. The reason
        // is that documents may have been deleted.
        std::atomic<size_t> m_documentCount;
        std::atomic<size_t> m_totalSourceByteSize;

        std::unique_ptr<DocumentMap> m_documentMap;

        std::unique_ptr<DocumentCache> m_documentCache;

        std::vector<std::unique_ptr<Shard>> m_shards;

        // TokenManager which distributes tokens for thread synchronization.
        std::unique_ptr<ITokenManager> m_tokenManager;

        // Lock protecting concurrent DeleteDocument operations.
        std::mutex m_deleteDocumentLock;


        DocumentHistogramBuilder m_histogram;

        // Allocator used to allocate memory for the slice buffers within
        // Shards. ISliceBufferAllocator uses IBlockAllocator to allocate
        // blocks of the same byte size. Slices within Shards will choose the
        // capacity for which the byte size of the buffer is sufficient.
        ISliceBufferAllocator& m_sliceBufferAllocator;
    };
}
