const API_URL = "http://localhost:8000";


/* =========================================================
   AUTHENTICATION STATE
   ========================================================= */

let currentUser = null;
let authToken = null;


/* =========================================================
   LOGIN
   ========================================================= */

async function login(event) {

    if (event) {
        event.preventDefault();
    }

    const usernameInput =
        document.getElementById("username");

    const passwordInput =
        document.getElementById("password");

    const errorElement =
        document.getElementById("login-error");

    const username =
        usernameInput.value.trim();

    const password =
        passwordInput.value;

    if (!username || !password) {
        showLoginError(
            "Please enter your username and password."
        );

        return;
    }

    errorElement.hidden = true;

    try {

        const response =
            await fetch(`${API_URL}/login`, {

                method: "POST",

                headers: {
                    "Content-Type": "application/json"
                },

                body: JSON.stringify({
                    username: username,
                    password: password
                })
            });


        const data =
            await response.json();


        if (!response.ok) {

            throw new Error(
                data.error ||
                "Invalid username or password."
            );
        }


        /*
         * Store authentication information.
         *
         * This token is used for protected API requests.
         */

        authToken = data.token;

        currentUser = {
            user: data.user,
            department: data.department,
            role: data.role
        };

        authToken = data.token;

        currentUser = {
            user: data.user,
            department: data.department,
            role: data.role
        };

        localStorage.setItem(
            "rag_auth_token",
            authToken
        );

        localStorage.setItem(
            "rag_user",
            JSON.stringify(currentUser)
        );

        showAuthenticatedApp();


        /*
         * Keep the session in browser storage so
         * refreshing the page does not immediately
         * lose the authenticated state.
         */

        localStorage.setItem(
            "rag_auth_token",
            authToken
        );

        localStorage.setItem(
            "rag_user",
            JSON.stringify(currentUser)
        );


        /*
         * Show application.
         */

        showAuthenticatedApp();

    } catch (error) {

        console.error(error);

        showLoginError(
            error.message ||
            "Login failed."
        );
    }
}


/* =========================================================
   LOGIN ERROR
   ========================================================= */

function showLoginError(message) {

    const errorElement =
        document.getElementById("login-error");

    if (!errorElement) {
        return;
    }

    errorElement.textContent = message;

    errorElement.hidden = false;
}


/* =========================================================
   RESTORE SESSION
   ========================================================= */

function restoreSession() {

    const storedToken =
        localStorage.getItem(
            "rag_auth_token"
        );

    const storedUser =
        localStorage.getItem(
            "rag_user"
        );


    if (!storedToken || !storedUser) {
        showLoginScreen();
        return;
    }


    try {

        const parsedUser =
            JSON.parse(storedUser);

        authToken = storedToken;

        currentUser = parsedUser;

        showAuthenticatedApp();

    } catch (error) {

        console.error(error);

        logout();
    }
}


/* =========================================================
   SHOW LOGIN SCREEN
   ========================================================= */

function showLoginScreen() {

    const loginScreen =
        document.getElementById("login-screen");

    const appScreen =
        document.getElementById("app-screen");


    if (loginScreen) {
        loginScreen.hidden = false;
    }

    if (appScreen) {
        appScreen.hidden = true;
    }
}


/* =========================================================
   SHOW APPLICATION
   ========================================================= */

function showAuthenticatedApp() {
    const loginScreen =
        document.getElementById("login-screen");

    const appScreen =
        document.getElementById("app-screen");

    if (loginScreen) {
        loginScreen.hidden = true;
    }

    if (appScreen) {
        appScreen.hidden = false;
    }

    updateUserInterface();
    updateAdminVisibility();

    // Redirect according to authenticated role.
    if (
        currentUser &&
        currentUser.role === "admin"
    ) {
        showSection("admin");
    } else {
        showSection("ask");
    }
}


/* =========================================================
   UPDATE USER INFORMATION
   ========================================================= */

function updateUserInterface() {

    if (!currentUser) {
        return;
    }


    const currentUserElement =
        document.getElementById("current-user");

    const currentRoleElement =
        document.getElementById("current-user-role");

    const avatarElement =
        document.getElementById("user-avatar-letter");

    const accessDepartment =
        document.getElementById("access-department");


    if (currentUserElement) {

        currentUserElement.textContent =
            currentUser.user;
    }


    if (currentRoleElement) {

        currentRoleElement.textContent =
            `${currentUser.department} · ${currentUser.role}`;
    }


    if (avatarElement) {

        avatarElement.textContent =
            currentUser.user
                .charAt(0)
                .toUpperCase();
    }


    if (accessDepartment) {

        accessDepartment.textContent =
            capitalize(currentUser.department);
    }
}


/* =========================================================
   ADMIN VISIBILITY
   ========================================================= */

function updateAdminVisibility() {

    const adminNavigation =
        document.getElementById("nav-admin");


    if (!adminNavigation) {
        return;
    }


    if (
        currentUser &&
        currentUser.role === "admin"
    ) {

        adminNavigation.style.display =
            "flex";

    } else {

        adminNavigation.style.display =
            "none";
    }
}


/* =========================================================
   LOGOUT
   ========================================================= */

function logout() {

    authToken = null;
    currentUser = null;


    localStorage.removeItem(
        "rag_auth_token"
    );

    localStorage.removeItem(
        "rag_user"
    );


    showLoginScreen();


    const usernameInput =
        document.getElementById("username");

    const passwordInput =
        document.getElementById("password");


    if (usernameInput) {
        usernameInput.value = "";
    }

    if (passwordInput) {
        passwordInput.value = "";
    }
}


/* =========================================================
   SECTION NAVIGATION
   ========================================================= */

function showSection(section) {

    const askSection =
        document.getElementById("ask-section");

    const adminSection =
        document.getElementById("admin-section");

    const askNavigation =
        document.getElementById("nav-ask");

    const adminNavigation =
        document.getElementById("nav-admin");


    /*
     * Employee / Knowledge Base
     */

    if (askSection) {

        askSection.style.display =
            section === "ask"
                ? "block"
                : "none";
    }


    /*
     * Admin section
     *
     * Extra client-side protection.
     * The backend still performs the real
     * authorization check.
     */

    if (adminSection) {

        if (
            section === "admin" &&
            currentUser &&
            currentUser.role === "admin"
        ) {

            adminSection.style.display =
                "block";

        } else {

            adminSection.style.display =
                "none";
        }
    }


    /*
     * Navigation highlighting
     */

    if (askNavigation) {

        askNavigation.classList.toggle(
            "active",
            section === "ask"
        );
    }


    if (adminNavigation) {

        adminNavigation.classList.toggle(
            "active",
            section === "admin"
        );
    }
}


/* =========================================================
   ASK KNOWLEDGE BASE
   ========================================================= */

async function askQuestion() {

    const queryInput =
        document.getElementById("query");

    const resultsContainer =
        document.getElementById("results");


    if (!queryInput || !resultsContainer) {
        return;
    }


    const query =
        queryInput.value.trim();


    if (!query) {

        queryInput.focus();

        return;
    }


    /*
     * User must be authenticated.
     */

    if (!authToken) {

        resultsContainer.innerHTML = `
            <div class="error-message">
                Please sign in again.
            </div>
        `;

        showLoginScreen();

        return;
    }


    /*
     * Loading state
     */

    resultsContainer.innerHTML = `
        <div class="loading-state">
            <div class="loading-spinner"></div>
            <span>
                Searching authorized documents...
            </span>
        </div>
    `;


    try {

        /*
         * IMPORTANT:
         *
         * We no longer send:
         *
         * user
         * department
         * role
         *
         * from the browser.
         *
         * The C++ backend gets those values
         * from the authenticated Bearer token.
         */

        const response =
            await fetch(`${API_URL}/query`, {

                method: "POST",

                headers: {
                    "Content-Type": "application/json",

                    "Authorization":
                        `Bearer ${authToken}`
                },

                body: JSON.stringify({
                    query: query,
                    k: 5
                })
            });


        const data =
            await response.json();


        /*
         * Authentication expired/invalid.
         */

        if (response.status === 401) {

            logout();

            showLoginError(
                data.error ||
                "Your session is no longer valid."
            );

            return;
        }


        if (!response.ok) {

            throw new Error(
                data.error ||
                `Request failed with status ${response.status}`
            );
        }


        displayResults(data);

    } catch (error) {

        console.error(error);

        resultsContainer.innerHTML = `
            <div class="error-message">

                <strong>
                    Search failed
                </strong>

                <p>
                    ${escapeHtml(error.message)}
                </p>

            </div>
        `;
    }
}


/* =========================================================
   DISPLAY RESULTS
   ========================================================= */

function displayResults(data) {

    const resultsContainer =
        document.getElementById("results");


    if (!resultsContainer) {
        return;
    }


    const results =
        data.results || [];


    if (results.length === 0) {

        resultsContainer.innerHTML = `
            <div class="empty-state">

                <div class="empty-icon">
                    ⌕
                </div>

                <h3>
                    No authorized sources found
                </h3>

                <p>
                    No relevant documents were found
                    within your access permissions.
                </p>

            </div>
        `;

        return;
    }


    resultsContainer.innerHTML = "";


    results.forEach(
        (result, index) => {

            const source =
                result.citation?.source ||
                "Unknown source";


            const chunk =
                result.citation?.chunk ??
                "—";


            const startLine =
                result.citation?.startLine;


            const endLine =
                result.citation?.endLine;


            const score =
                typeof result.score === "number"
                    ? result.score.toFixed(3)
                    : "—";


            /*
             * Citation display
             */

            let citationDetails =
                `chunk ${escapeHtml(
                    String(chunk)
                )}`;


            if (
                startLine !== undefined &&
                endLine !== undefined
            ) {

                citationDetails +=
                    ` · lines ${escapeHtml(
                        String(startLine)
                    )}–${escapeHtml(
                        String(endLine)
                    )}`;
            }


            const card =
                document.createElement("div");


            card.className =
                "result-card";


            card.innerHTML = `

                <div class="result-top">

                    <div class="source-label">

                        <span class="source-icon">
                            📄
                        </span>

                        <span>
                            Source ${index + 1}
                        </span>

                    </div>


                    <span class="score">
                        Relevance ${escapeHtml(score)}
                    </span>

                </div>


                <div class="result-text">
                    ${escapeHtml(
                        result.text || ""
                    )}
                </div>


                <div class="citation">

                    <span>
                        Source:
                    </span>

                    <strong>
                        ${escapeHtml(source)}
                    </strong>

                    <span>
                        · ${citationDetails}
                    </span>

                </div>

            `;


            resultsContainer.appendChild(card);
        }
    );
}


/* =========================================================
   ADMIN DOCUMENT UPLOAD
   ========================================================= */

async function uploadDocument() {

    const fileInput =
        document.getElementById(
            "documentFile"
        );

    const departmentInput =
        document.getElementById(
            "documentDepartment"
        );

    const roleInput =
        document.getElementById(
            "documentRole"
        );

    const uploadStatus =
        document.getElementById(
            "uploadStatus"
        );


    if (
        !fileInput ||
        !departmentInput ||
        !roleInput ||
        !uploadStatus
    ) {
        return;
    }


    /*
     * Client-side admin check.
     */

    if (
        !currentUser ||
        currentUser.role !== "admin"
    ) {

        uploadStatus.innerHTML = `
            <div class="error-message">
                Administrator access required.
            </div>
        `;

        return;
    }


    if (
        !fileInput.files ||
        fileInput.files.length === 0
    ) {

        uploadStatus.innerHTML = `
            <div class="error-message">
                Please select a document first.
            </div>
        `;

        return;
    }


    const file =
        fileInput.files[0];


    /*
     * Only allow TXT and Markdown files.
     */

    const filename =
        file.name.toLowerCase();


    if (
        !filename.endsWith(".txt") &&
        !filename.endsWith(".md")
    ) {

        uploadStatus.innerHTML = `
            <div class="error-message">
                Only .txt and .md files are supported.
            </div>
        `;

        return;
    }


    const content =
        await file.text();


    if (!content.trim()) {

        uploadStatus.innerHTML = `
            <div class="error-message">
                The selected document is empty.
            </div>
        `;

        return;
    }


    uploadStatus.innerHTML = `
        <div class="loading-state">

            <div class="loading-spinner"></div>

            <span>
                Indexing document...
            </span>

        </div>
    `;


    try {

        /*
         * IMPORTANT:
         *
         * Admin authentication is now sent
         * through the Bearer token.
         */

        const response =
            await fetch(`${API_URL}/documents`, {

                method: "POST",

                headers: {

                    "Content-Type":
                        "application/json",

                    "Authorization":
                        `Bearer ${authToken}`
                },

                body: JSON.stringify({

                    filename: file.name,

                    content: content,

                    department:
                        departmentInput.value,

                    role:
                        roleInput.value
                })
            });


        const data =
            await response.json();


        /*
         * Authentication failure.
         */

        if (response.status === 401) {

            logout();

            showLoginError(
                data.error ||
                "Your session is no longer valid."
            );

            return;
        }


        /*
         * Authorization failure.
         */

        if (response.status === 403) {

            uploadStatus.innerHTML = `
                <div class="error-message">

                    <strong>
                        Access denied
                    </strong>

                    <p>
                        ${escapeHtml(
                            data.error ||
                            "Administrator access required."
                        )}
                    </p>

                </div>
            `;

            return;
        }


        if (!response.ok) {

            throw new Error(
                data.error ||
                "Document upload failed."
            );
        }


        /*
         * Success
         */

        uploadStatus.innerHTML = `

            <div class="success-message">

                <strong>
                    Document indexed successfully.
                </strong>

                <p>
                    ${escapeHtml(
                        data.filename ||
                        file.name
                    )}
                </p>

                <p>
                    Department:
                    ${escapeHtml(
                        data.department ||
                        departmentInput.value
                    )}

                    ·

                    Role:
                    ${escapeHtml(
                        data.role ||
                        roleInput.value
                    )}
                </p>

                ${
                    data.uri
                        ? `
                            <p>
                                Source:
                                ${escapeHtml(data.uri)}
                            </p>
                          `
                        : ""
                }

            </div>
        `;


        /*
         * Clear selected file.
         */

        fileInput.value = "";


    } catch (error) {

        console.error(error);

        uploadStatus.innerHTML = `

            <div class="error-message">

                <strong>
                    Upload failed
                </strong>

                <p>
                    ${escapeHtml(
                        error.message
                    )}
                </p>

            </div>
        `;
    }
}


/* =========================================================
   HTML ESCAPING
   ========================================================= */

function escapeHtml(value) {

    return String(value)

        .replace(
            /&/g,
            "&amp;"
        )

        .replace(
            /</g,
            "&lt;"
        )

        .replace(
            />/g,
            "&gt;"
        )

        .replace(
            /"/g,
            "&quot;"
        )

        .replace(
            /'/g,
            "&#039;"
        );
}


/* =========================================================
   CAPITALIZE
   ========================================================= */

function capitalize(value) {

    if (!value) {
        return "";
    }

    return String(value)
        .charAt(0)
        .toUpperCase()
        + String(value).slice(1);
}


/* =========================================================
   ADMIN RETRIEVAL COMPARISON
   ========================================================= */

async function compareRetrieval() {
    const queryInput = document.getElementById("comparisonQuery");
    const output = document.getElementById("comparisonResults");
    const status = document.getElementById("comparisonStatus");
    const button = document.getElementById("compareButton");
    if (!queryInput || !output || !status || !button) return;
    const query = queryInput.value.trim();
    if (!query) {
        queryInput.focus();
        status.innerHTML = '<p class="comparison-message error-message">Enter a query to compare retrieval strategies.</p>';
        return;
    }
    if (!authToken || !currentUser || currentUser.role !== "admin") {
        status.innerHTML = '<p class="comparison-message error-message">Administrator sign-in is required.</p>';
        return;
    }
    button.disabled = true;
    output.innerHTML = "";
    status.innerHTML = '<div class="loading-state"><div class="loading-spinner"></div><span>Running all three authorized retrieval paths…</span></div>';
    try {
        const response = await fetch(`${API_URL}/compare`, {
            method: "POST",
            headers: { "Content-Type": "application/json", "Authorization": `Bearer ${authToken}` },
            body: JSON.stringify({ query, k: 5 })
        });
        const data = await response.json();
        if (response.status === 401) {
            logout();
            showLoginError(data.error || "Your session is no longer valid.");
            return;
        }
        if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);

        const strategies = [
            ["dense", `Dense · ${data.denseIndex}`, "Cosine similarity"],
            ["bm25", "Keyword · BM25", "BM25 score"],
            ["hybrid", "Hybrid · RRF", `RRF score · k=${data.rrfK}`]
        ];
        const safe = (value) => escapeHtml(value == null ? "" : String(value));
        const cards = strategies.map(([key, title, metric]) => {
            const lane = data.strategies[key];
            const docs = (lane.results || []).map((item) => `
                <article class="comparison-result">
                    <div class="comparison-result-meta"><span>Rank ${safe(item.rank)}</span><strong>${safe(Number(item.score).toFixed(5))}</strong></div>
                    <p>${safe(item.text)}</p>
                    <small>${safe(item.citation.source)} · chunk ${safe(item.citation.chunk)}</small>
                </article>
            `).join("") || '<p class="comparison-empty">No authorized matches.</p>';
            return `<section class="comparison-strategy"><header><div><h4>${title}</h4><small>${metric}</small></div><strong>${Number(lane.latencyMs).toFixed(2)} ms</strong></header><div class="comparison-result-list">${docs}</div></section>`;
        }).join("");
        const common = data.commonChunkIds || [];
        const unique = data.uniqueChunkIds || {};
        const overlap = data.overlapCounts || {};
        status.innerHTML = '<p class="comparison-message success-message">Comparison complete. Each lane used the authenticated administrator’s document access filter.</p>';
        output.innerHTML = `
            <div class="comparison-summary">
                <div><span>Common to all three</span><strong>${common.length}</strong><small>chunks</small></div>
                <div><span>BM25 ∩ Dense</span><strong>${overlap.bm25Dense || 0}</strong><small>chunks</small></div>
                <div><span>BM25 ∩ Hybrid</span><strong>${overlap.bm25Hybrid || 0}</strong><small>chunks</small></div>
                <div><span>Dense ∩ Hybrid</span><strong>${overlap.denseHybrid || 0}</strong><small>chunks</small></div>
                <div><span>BM25 only</span><strong>${(unique.bm25 || []).length}</strong><small>chunks</small></div>
                <div><span>Dense only</span><strong>${(unique.dense || []).length}</strong><small>chunks</small></div>
                <div><span>Hybrid only</span><strong>${(unique.hybrid || []).length}</strong><small>chunks</small></div>
            </div>
            <div class="comparison-grid">${cards}</div>`;
    } catch (error) {
        status.innerHTML = `<p class="comparison-message error-message">${escapeHtml(error.message)}</p>`;
    } finally {
        button.disabled = false;
    }
}


/* =========================================================
   KEYBOARD SHORTCUT
   ========================================================= */

document.addEventListener(
    "keydown",
    (event) => {

        /*
         * Ctrl + Enter submits the query.
         */

        if (
            event.ctrlKey &&
            event.key === "Enter"
        ) {

            const queryInput =
                document.getElementById(
                    "query"
                );

            if (
                queryInput &&
                document.activeElement ===
                    queryInput
            ) {

                askQuestion();
            }
        }
    }
);


/* =========================================================
   INITIALIZATION
   ========================================================= */

document.addEventListener(
    "DOMContentLoaded",
    () => {

        /*
         * Login form
         */

        const loginForm =
            document.getElementById(
                "login-form"
            );


        if (loginForm) {

            loginForm.addEventListener(
                "submit",
                login
            );
        }


        /*
         * Logout button
         */

        const logoutButton =
            document.getElementById(
                "logout-button"
            );


        if (logoutButton) {

            logoutButton.addEventListener(
                "click",
                logout
            );
        }


        /*
         * Restore previous authenticated
         * session if one exists.
         */

        restoreSession();
    }
);
