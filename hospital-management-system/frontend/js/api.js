/**
 * Hospital Management System - Unified API Client
 * Communicates directly with C CGI backend endpoints
 */

const API_BASE = "/api";

const API = {
    getToken() {
        return localStorage.getItem("hms_token") || "";
    },

    setToken(token) {
        if (token) {
            localStorage.setItem("hms_token", token);
        } else {
            localStorage.removeItem("hms_token");
        }
    },

    getUser() {
        const u = localStorage.getItem("hms_user");
        try {
            return u ? JSON.parse(u) : null;
        } catch {
            return null;
        }
    },

    setUser(user) {
        if (user) {
            localStorage.setItem("hms_user", JSON.stringify(user));
        } else {
            localStorage.removeItem("hms_user");
        }
    },

    async request(endpoint, options = {}) {
        const url = `${API_BASE}${endpoint}`;
        const headers = {
            "Content-Type": "application/json",
            ...(options.headers || {})
        };

        const token = this.getToken();
        if (token) {
            headers["X-Session-Token"] = token;
            headers["Authorization"] = `Bearer ${token}`;
        }

        const config = {
            method: options.method || "GET",
            headers,
            credentials: "include"
        };

        if (options.body) {
            config.body = typeof options.body === "string" ? options.body : JSON.stringify(options.body);
        }

        try {
            const resp = await fetch(url, config);
            const data = await resp.json();

            if (!resp.ok) {
                if (resp.status === 401) {
                    const currentPage = window.location.pathname;
                    if (!currentPage.endsWith("login.html") && !currentPage.endsWith("register.html") && !currentPage.endsWith("index.html") && currentPage !== "/") {
                        this.setToken(null);
                        this.setUser(null);
                        window.location.href = "login.html?expired=1";
                    }
                }
                throw new Error(data.message || `Request failed with status ${resp.status}`);
            }

            return data;
        } catch (err) {
            console.error(`API Error [${endpoint}]:`, err);
            throw err;
        }
    },

    get(endpoint) {
        return this.request(endpoint, { method: "GET" });
    },

    post(endpoint, body) {
        return this.request(endpoint, { method: "POST", body });
    },

    put(endpoint, body) {
        return this.request(endpoint, { method: "PUT", body });
    },

    delete(endpoint) {
        return this.request(endpoint, { method: "DELETE" });
    }
};

// UI Notification Toasts
function showToast(message, type = "info") {
    let container = document.getElementById("toast-container");
    if (!container) {
        container = document.createElement("div");
        container.id = "toast-container";
        document.body.appendChild(container);
    }

    const toast = document.createElement("div");
    toast.className = `toast-msg toast-${type}`;
    toast.innerHTML = `<span>${message}</span> <span style="cursor:pointer; margin-left:10px;" onclick="this.parentElement.remove()">&times;</span>`;

    container.appendChild(toast);
    setTimeout(() => {
        toast.style.opacity = "0";
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

