/**
 * Hospital Management System - Authentication & Role Navigation Guard
 */

const Auth = {
    async checkAuth(allowedRoles = []) {
        const token = API.getToken();
        if (!token) {
            window.location.href = "login.html";
            return null;
        }

        try {
            const res = await API.get("/auth/me");
            const user = res.data;
            API.setUser(user);

            if (allowedRoles.length > 0 && !allowedRoles.includes(user.role) && user.role !== "admin") {
                showToast("Access restricted: You do not have permission for this section.", "error");
                setTimeout(() => {
                    this.redirectToDashboard(user.role);
                }, 1000);
                return null;
            }

            return user;
        } catch (err) {
            console.error("Auth check failed:", err);
            API.setToken(null);
            API.setUser(null);
            window.location.href = "login.html?expired=1";
            return null;
        }
    },

    redirectToDashboard(role) {
        switch (role) {
            case "admin":
                window.location.href = "admin-dashboard.html";
                break;
            case "doctor":
                window.location.href = "doctor-dashboard.html";
                break;
            case "receptionist":
                window.location.href = "receptionist-dashboard.html";
                break;
            case "patient":
                window.location.href = "patient-dashboard.html";
                break;
            default:
                window.location.href = "index.html";
        }
    },

    async logout() {
        try {
            await API.post("/auth/logout", {});
        } catch (err) {
            console.warn("Logout error:", err);
        } finally {
            API.setToken(null);
            API.setUser(null);
            window.location.href = "login.html";
        }
    },

    renderNavigation(activePageId) {
        const user = API.getUser() || { username: "Guest", role: "User" };
        const sidebarContainer = document.getElementById("sidebar-container");
        if (!sidebarContainer) return;

        let navItems = [];

        if (user.role === "admin") {
            navItems = [
                { id: "dashboard", label: "Dashboard", href: "admin-dashboard.html", icon: "📊" },
                { id: "patients", label: "Patients", href: "patients.html", icon: "👥" },
                { id: "doctors", label: "Doctors", href: "doctors.html", icon: "🩺" },
                { id: "departments", label: "Departments", href: "departments.html", icon: "🏥" },
                { id: "appointments", label: "Appointments", href: "appointments.html", icon: "📅" },
                { id: "records", label: "Medical Records", href: "medical-records.html", icon: "📋" },
                { id: "prescriptions", label: "Prescriptions", href: "prescriptions.html", icon: "💊" },
                { id: "pharmacy", label: "Pharmacy Stock", href: "pharmacy.html", icon: "🏬" },
                { id: "billing", label: "Billing & Invoices", href: "billing.html", icon: "🧾" },
                { id: "emergency", label: "Emergency Queue", href: "emergency.html", icon: "🚨" },
                { id: "reports", label: "Hospital Reports", href: "reports.html", icon: "📈" }
            ];
        } else if (user.role === "doctor") {
            navItems = [
                { id: "dashboard", label: "Doctor Dashboard", href: "doctor-dashboard.html", icon: "🩺" },
                { id: "appointments", label: "My Appointments", href: "appointments.html", icon: "📅" },
                { id: "patients", label: "Patient Directory", href: "patients.html", icon: "👥" },
                { id: "records", label: "Medical Records (EMR)", href: "medical-records.html", icon: "📋" },
                { id: "prescriptions", label: "Prescriptions", href: "prescriptions.html", icon: "💊" },
                { id: "emergency", label: "Emergency Queue", href: "emergency.html", icon: "🚨" }
            ];
        } else if (user.role === "receptionist") {
            navItems = [
                { id: "dashboard", label: "Reception Dashboard", href: "receptionist-dashboard.html", icon: "🛎️" },
                { id: "patients", label: "Register / Search Patients", href: "patients.html", icon: "👥" },
                { id: "appointments", label: "Book Appointments", href: "appointments.html", icon: "📅" },
                { id: "doctors", label: "Doctor Availability", href: "doctors.html", icon: "🩺" },
                { id: "billing", label: "Billing & Cashier", href: "billing.html", icon: "🧾" },
                { id: "emergency", label: "Admit Emergency", href: "emergency.html", icon: "🚨" }
            ];
        } else if (user.role === "patient") {
            navItems = [
                { id: "dashboard", label: "Patient Portal", href: "patient-dashboard.html", icon: "👤" },
                { id: "appointments", label: "My Appointments", href: "appointments.html", icon: "📅" },
                { id: "doctors", label: "Find Doctors", href: "doctors.html", icon: "🩺" },
                { id: "departments", label: "Specialties", href: "departments.html", icon: "🏥" },
                { id: "records", label: "My Health Records", href: "medical-records.html", icon: "📋" },
                { id: "prescriptions", label: "My Prescriptions", href: "prescriptions.html", icon: "💊" },
                { id: "billing", label: "My Invoices & Receipts", href: "billing.html", icon: "🧾" }
            ];
        }

        const navHtml = navItems.map(item => `
            <a href="${item.href}" class="nav-item ${item.id === activePageId ? 'active' : ''}">
                <span class="icon">${item.icon}</span>
                <span>${item.label}</span>
            </a>
        `).join("");

        sidebarContainer.innerHTML = `
            <aside class="sidebar">
                <div class="sidebar-header">
                    <div class="logo-icon">+</div>
                    <div>
                        <h2>MEDICARE HMS</h2>
                        <small>C Backend Engine</small>
                    </div>
                </div>
                <div class="sidebar-nav">
                    <div class="nav-section-title">Navigation</div>
                    ${navHtml}
                    <div class="nav-section-title" style="margin-top:15px;">Account</div>
                    <a href="profile.html" class="nav-item ${activePageId === 'profile' ? 'active' : ''}">
                        <span class="icon">⚙️</span>
                        <span>My Profile</span>
                    </a>
                    <a href="javascript:void(0)" onclick="Auth.logout()" class="nav-item" style="color:#f87171;">
                        <span class="icon">🚪</span>
                        <span>Logout</span>
                    </a>
                </div>
                <div class="sidebar-footer">
                    <div class="user-card">
                        <div class="user-avatar">${(user.username || 'U')[0].toUpperCase()}</div>
                        <div class="user-info">
                            <div class="user-name">${user.username || 'User'}</div>
                            <span class="user-role-badge">${user.role || 'Member'}</span>
                        </div>
                    </div>
                </div>
            </aside>
        `;
    }
};

