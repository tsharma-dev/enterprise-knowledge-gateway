# Private Enterprise Knowledge Gateway

> A production-grade, permission-aware hybrid RAG platform built in modern C++23.

A secure enterprise knowledge retrieval system combining **hybrid lexical + semantic search, HNSW approximate nearest-neighbor retrieval, document-level authorization, PostgreSQL authentication, provenance-aware citations, REST APIs, administrative ingestion, and retrieval/security evaluation**.

The system is designed around one core principle:

> **Relevant information should only be returned when the requesting user is authorized to access it.**

---

## 🚀 Highlights

- 🔐 Document-level access control
- 👤 PostgreSQL-backed authentication
- 🧠 Hybrid BM25 + dense vector retrieval
- ⚡ HNSW approximate nearest-neighbor search
- 🎯 Metadata-filtered retrieval
- 📎 Provenance-aware citations
- 🌐 REST API
- 💻 Web-based enterprise interface
- 📤 Administrative document ingestion
- 📊 Retrieval quality evaluation
- 🛡️ Authorization leakage testing
- 💾 Persistent `.ragdb` knowledge indexes
- 🧩 Pluggable embedding backends
- 🚀 Modern C++23 implementation

---

# 🏗️ Architecture

```text
                         ┌──────────────────────┐
                         │        USER          │
                         └──────────┬───────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │    WEB FRONTEND      │
                         │      HTML/CSS/JS     │
                         └──────────┬───────────┘
                                    │
                                  REST
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │ ENTERPRISE GATEWAY   │
                         │        C++23         │
                         └──────────┬───────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │     POSTGRESQL       │
                         │    AUTHENTICATION    │
                         └──────────┬───────────┘
                                    │
                              User Context
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │   AUTHORIZATION      │
                         │       POLICY         │
                         │                      │
                         │ Department + Role    │
                         └──────────┬───────────┘
                                    │
                              Metadata Filter
                                    │
                                    ▼
              ┌────────────────────────────────────────┐
              │             RAG RETRIEVAL              │
              │                                        │
              │       ┌───────────────┐                │
              │       │      BM25     │                │
              │       └───────┬───────┘                │
              │               │                         │
              │               ├──► HYBRID FUSION       │
              │               │                         │
              │       ┌───────▼───────┐                │
              │       │ DENSE / HNSW  │                │
              │       └───────────────┘                │
              └────────────────────┬───────────────────┘
                                   │
                                   ▼
                         ┌──────────────────────┐
                         │     PROVENANCE       │
                         │                      │
                         │ Source               │
                         │ Chunk                │
                         │ Score                │
                         │ Line Range           │
                         └──────────────────────┘
```

---

# 📈 Evaluation Results

The enterprise evaluation dataset contains **20 queries** across engineering, finance, HR, and company-wide policies.

| Retrieval Mode | Recall@5 | MRR@5 | Unauthorized Results |
|---|---:|---:|---:|
| BM25 | **1.000** | **1.000** | **0** |
| Dense | **1.000** | **0.975** | **0** |
| Hybrid | **1.000** | **1.000** | **0** |

### Security Results

```text
Queries evaluated:       20
Unauthorized results:     0
Authorization leakage:   0%
Security check:        PASSED
```

The evaluation measures both **retrieval quality** and **security correctness**.

---

# 🔐 Security Model

Enterprise knowledge bases contain information that should not be visible to every employee.

The gateway associates users and documents with authorization metadata.

## User Context

```cpp
struct UserContext {
    std::string user_id;
    std::string department;
    std::string role;
};
```

## Document Metadata

Example:

```text
department = engineering
role = employee
```

The authorization policy converts the authenticated user's context into a metadata filter before retrieval results are returned.

---

## Authorization Rules

```text
PUBLIC DOCUMENT
       │
       └──► Accessible to all authorized users


DEPARTMENT DOCUMENT
       │
       └──► Department must match


ROLE-RESTRICTED DOCUMENT
       │
       └──► Department + role must match
```

This makes access control part of the retrieval process rather than a presentation-layer feature.

---

# 🔑 Authentication Flow

The client does not choose its own department or role.

Instead:

```text
             Bearer Token
                  │
                  ▼
        ┌──────────────────┐
        │   PostgreSQL     │
        │  Authentication  │
        └────────┬─────────┘
                 │
                 ▼
            User Context
                 │
                 ▼
        ┌──────────────────┐
        │ Authorization    │
        │     Policy       │
        └────────┬─────────┘
                 │
                 ▼
        Permission-Aware
           Retrieval
```

This prevents a client from simply claiming:

```json
{
  "department": "finance",
  "role": "employee"
}
```

and gaining access to finance information.

---

# 🧠 Hybrid Retrieval

The retrieval pipeline combines lexical and semantic search.

## BM25

BM25 provides strong lexical matching for queries containing:

- Exact terminology
- Names
- Policy phrases
- Numbers
- Technical keywords

## Dense Retrieval

Dense retrieval uses vector embeddings to identify semantically related content even when the wording differs.

The semantic pipeline uses:

```text
Ollama
   │
   ▼
nomic-embed-text
   │
   ▼
Dense Embedding
   │
   ▼
HNSW Search
```

## Hybrid Retrieval

The hybrid retrieval path combines:

```text
        Query
          │
     ┌────┴────┐
     ▼         ▼
   BM25      Dense
     │         │
     └────┬────┘
          ▼
     Hybrid Fusion
          │
          ▼
   Ranked Results
```

---

# 📎 Provenance-Aware Citations

Every retrieval result contains provenance information so users can understand where retrieved knowledge came from.

Example:

```text
[1] Source
    uri: enterprise_documents.csv#finance
    chunk: 0
    score: 1.000
    lines: 1-1
    text: Expenses above 100000 INR require director approval.
```

The REST API exposes structured citation information:

```json
{
  "citation": {
    "source": "enterprise_documents.csv#finance",
    "startLine": 1,
    "endLine": 1
  }
}
```

This makes retrieval results traceable instead of presenting opaque information.

---

# 🌐 REST API

The enterprise gateway exposes a lightweight HTTP interface.

## Health Check

```http
GET /health
```

Example response:

```json
{
  "service": "enterprise-knowledge-gateway",
  "status": "ok"
}
```

---

## Query

```http
POST /query
Authorization: Bearer <token>
Content-Type: application/json
```

Request:

```json
{
  "query": "What expenses require director approval?",
  "k": 5
}
```

Where:

```text
k = number of top retrieval results requested
```

The server authenticates the user and applies authorization before returning retrieval results.

---

## Document Upload

Administrative users can upload documents through:

```http
POST /documents
Authorization: Bearer <admin-token>
```

Uploaded documents are indexed with department and role metadata.

Non-administrative users are denied access to administrative operations.

---

# 💻 Web Interface

The project includes a browser-based enterprise interface.

### Authentication

Users sign in through a secure login interface.

### Knowledge Search

Users can ask natural-language questions against the enterprise knowledge base.

### Retrieved Results

Results display:

- Relevance score
- Retrieved text
- Document identifier
- Source information
- Line range
- Citation metadata

### Administration

Administrative users can upload additional documents to the knowledge base.

---

# 🛡️ Security Demonstration

The same question can produce different results depending on the authenticated user's permissions.

## Engineering User

Question:

```text
What expenses require director approval?
```

Finance information is excluded from the retrieval results.

Returned documents remain within the engineering/public authorization scope.

---

## Finance User

The same question:

```text
What expenses require director approval?
```

returns:

```text
enterprise_documents.csv#finance

Expenses above 100000 INR require director approval.
```

---

## Permission Boundary

```text
Engineering Employee
        │
        ├── Engineering documents    ✓
        ├── Public documents         ✓
        └── Finance documents        ✗


Finance Employee
        │
        ├── Finance documents        ✓
        ├── Public documents         ✓
        └── Engineering-private      ✗
```

The same query therefore produces different authorized result sets depending on the authenticated user's permissions.

---

# 📊 Retrieval & Security Evaluation

The evaluation dataset is located under:

```text
evaluation/
├── documents/
│   ├── engineering.txt
│   ├── finance.txt
│   ├── hr.txt
│   └── public.txt
│
├── enterprise_documents.csv
├── queries.json
└── qrels.json
```

The dataset contains:

- Engineering policies
- Finance policies
- HR policies
- Company-wide policies
- 20 evaluation queries
- Relevance judgments

---

# 📈 Benchmark Metrics

## Recall@K

Measures whether relevant documents appear within the top K retrieved results.

## MRR@K

Measures how highly the first relevant result appears.

## Authorization Leakage

Measures whether retrieval returns documents that the authenticated user is not permitted to access.

```text
Authorization Leakage =
Unauthorized Results / Total Retrieved Results
```

Target:

```text
0%
```

---

# 🧪 Running the Evaluation

Build the project:

```powershell
cmake -B build
cmake --build build
```

Then run:

```powershell
.\build\bench\ragcpp_enterprise_eval.exe
```

Expected summary:

```text
========================================
 Enterprise Retrieval Evaluation
========================================

Loaded queries: 20
Loaded qrels:   20

BM25
----------------------------------------
Recall@5:              1
MRR@5:                 1
Unauthorized results: 0
Authorization leakage: 0%

Dense
----------------------------------------
Recall@5:              1
MRR@5:                 0.975
Unauthorized results: 0
Authorization leakage: 0%

Hybrid
----------------------------------------
Recall@5:              1
MRR@5:                 1
Unauthorized results: 0
Authorization leakage: 0%

========================================
 Security Summary
========================================
Unauthorized results across all modes: 0
Authorization leakage: 0%
Security check passed.
```

---

# 📁 Project Structure

```text
rag-cpp/
│
├── include/
│   └── rag/
│       └── security/
│           └── authorization.hpp
│
├── src/
│   └── security/
│       └── authorization.cpp
│
├── cli/
│   └── enterprise_server.cpp
│
├── bench/
│   └── enterprise_eval_bench.cpp
│
├── tests/
│   └── authorization_tests.cpp
│
├── evaluation/
│   ├── documents/
│   │   ├── engineering.txt
│   │   ├── finance.txt
│   │   ├── hr.txt
│   │   └── public.txt
│   │
│   ├── enterprise_documents.csv
│   ├── queries.json
│   └── qrels.json
│
├── web/
│   ├── index.html
│   ├── style.css
│   └── app.js
│
├── CMakeLists.txt
└── README.md
```

---

# 🛠️ Technology Stack

## Backend

- C++23
- CMake
- HTTP REST API
- HNSW
- BM25
- Dense vector retrieval
- Hybrid retrieval
- Metadata filtering

## Database

- PostgreSQL 17

## AI / Embeddings

- Ollama
- `nomic-embed-text`

## Frontend

- HTML
- CSS
- JavaScript

## Security

- Bearer-token authentication
- PostgreSQL authentication
- Department-based authorization
- Role-based authorization
- Document-level access control

## Evaluation

- Recall@K
- MRR@K
- Authorization leakage
- Security regression testing

---

# 🚀 Installation

## Requirements

- C++23 compiler
- CMake 3.24+
- PostgreSQL 17
- Ollama

---

## Build

```powershell
cmake -B build
cmake --build build
```

Run tests:

```powershell
ctest --test-dir build
```

---

# 🤖 Ollama Setup

Install Ollama and download the embedding model:

```powershell
ollama pull nomic-embed-text
```

The embedding service is expected at:

```text
http://127.0.0.1:11434
```

---

# 🗄️ PostgreSQL Configuration

Configure the database connection using environment variables:

```powershell
$env:RAG_DB_HOST="127.0.0.1"
$env:RAG_DB_PORT="5432"
$env:RAG_DB_NAME="knowledge_gateway"
$env:RAG_DB_USER="postgres"
$env:RAG_DB_PASSWORD="YOUR_PASSWORD"
```

Never commit database passwords or authentication secrets to GitHub.

---

# ▶️ Start the Gateway

After building:

```powershell
.\build\cli\ragcpp_enterprise_server.exe
```

The gateway runs on:

```text
http://127.0.0.1:8000
```

---

# ❤️ Health Check

```powershell
Invoke-WebRequest `
    http://127.0.0.1:8000/health `
    -UseBasicParsing
```

Expected:

```json
{
  "service": "enterprise-knowledge-gateway",
  "status": "ok"
}
```

---

# 🔎 CLI Query

The retrieval engine can also be accessed through the CLI.

Example:

```powershell
.\build\cli\ragcpp.exe `
    query .\enterprise_eval.ragdb `
    "How many days of annual leave can engineering employees take?" `
    -k 5 `
    --user=alice `
    --department=engineering `
    --role=employee
```

Results include relevance information and provenance metadata.

---

# 💾 Persistent Knowledge Index

Knowledge bases are persisted using the `.ragdb` format.

Example:

```text
enterprise_eval.ragdb

documents: 4
chunks:    8
embeddings: yes
backend:   Ollama / nomic-embed-text
```

This allows indexed knowledge to be persisted and reopened without rebuilding the complete corpus.

---

# 🧩 Retrieval Capabilities

The retrieval system provides a broad set of information retrieval capabilities.

## Retrieval

- BM25
- Dense vector search
- Hybrid retrieval
- HNSW ANN
- Filtered HNSW
- Reciprocal Rank Fusion
- Relative Score Fusion
- Cross-encoder reranking
- Query expansion
- Parent-document stitching

## Advanced Retrieval

- SPLADE-style sparse retrieval
- ColBERT late interaction
- RAPTOR-style hierarchical retrieval
- HyDE
- Multi-query retrieval
- RAG-Fusion
- Corrective RAG
- Self-RAG
- MMR diversity reranking

## Graph Retrieval

- Document graphs
- Similarity edges
- Link edges
- Community detection
- Personalized PageRank
- Local graph search
- Global graph search

## Chunking

- Semantic chunking
- Proposition chunking
- Code-aware chunking
- Structure inference
- Contextual retrieval

## Persistence

- Versioned `.ragdb` format
- CRC validation
- Persistent document identifiers
- Incremental document deletion
- Embedding caches
- Query caches

---

# 🔌 Pluggable Embeddings

Supported embedding backends include:

```text
Ollama
OpenAI-compatible APIs
llama.cpp
Deterministic Hash embeddings
ONNX Runtime
GGUF / llama.cpp
```

Decorators include:

```text
RetryingEmbedder
FallbackEmbedder
```

This allows different embedding backends to be selected without changing the retrieval architecture.

---

# 📦 Source Loaders

The knowledge ingestion pipeline supports multiple document formats and data sources.

Supported formats include:

- TXT
- Markdown
- HTML
- CSV
- PDF
- Word documents
- Excel
- PowerPoint
- Source code
- Additional registered extractors

CSV columns can also be represented as filterable metadata.

---

# 💡 Why Permission-Aware RAG?

Traditional RAG pipelines generally focus on:

```text
Question
   ↓
Retrieve documents
   ↓
Generate answer
```

Enterprise systems require:

```text
Question
   ↓
Authenticate user
   ↓
Determine permissions
   ↓
Filter accessible knowledge
   ↓
Retrieve relevant documents
   ↓
Attach provenance
   ↓
Generate grounded response
```

This architecture treats **security as part of knowledge retrieval**, rather than as a separate UI concern.

---

# 🎯 Design Goals

## 1. Relevance

Return the most useful information for a user's question.

## 2. Security

Never expose documents outside the user's authorization boundary.

## 3. Traceability

Every retrieved result should have identifiable provenance.

## 4. Performance

Use efficient lexical and approximate-nearest-neighbor retrieval.

## 5. Measurability

Evaluate retrieval quality and security using reproducible tests.

---

# 🧪 Authorization Tests

The project includes dedicated authorization tests covering:

- Engineering users
- HR users
- Finance users
- Public documents
- Role restrictions
- Missing metadata
- Department mismatch
- Authorized access

Run:

```powershell
ctest --test-dir build
```

The authorization suite validates that users cannot retrieve documents outside their permitted department or role.

---

# 🔮 Future Improvements

Potential production improvements include:

- TLS
- Enterprise identity-provider integration
- Fine-grained ACLs
- Audit logging
- Rate limiting
- Concurrent request processing
- Request timeouts
- Incremental re-indexing
- Larger evaluation datasets
- Docker deployment
- Observability and metrics
- Distributed retrieval

---

# 📚 Research Foundations

The retrieval architecture draws from research and systems including:

- Retrieval-Augmented Generation
- BM25
- HNSW
- Reciprocal Rank Fusion
- ColBERT
- SPLADE
- GraphRAG
- RAPTOR
- HyDE
- Corrective RAG
- Self-RAG
- BEIR

The evaluation philosophy emphasizes **measured retrieval quality rather than unsupported accuracy claims**.

---

# 📌 Project Summary

```text
┌─────────────────────────────────────────────┐
│       PRIVATE ENTERPRISE KNOWLEDGE         │
│                  GATEWAY                    │
├─────────────────────────────────────────────┤
│                                             │
│  Authentication       PostgreSQL            │
│  Authorization        Department + Role     │
│  Retrieval            BM25 + Dense + HNSW   │
│  Embeddings           Ollama                │
│  API                  REST                  │
│  Frontend             HTML / CSS / JS       │
│  Provenance           Source + Chunk + Lines│
│  Evaluation           Recall / MRR          │
│  Security             0% Leakage            │
│                                             │
└─────────────────────────────────────────────┘
```

---

# 📄 Resume Description

### Private Enterprise Knowledge Gateway

> Developed a permission-aware enterprise RAG platform in C++23 combining BM25 and dense HNSW retrieval with PostgreSQL authentication, document-level authorization, provenance-aware citations, REST APIs, and retrieval/security benchmarking. Achieved 100% Recall@5 and 0% authorization leakage across a 20-query enterprise evaluation set.

---

# 📜 License

MIT