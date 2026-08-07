(() => {
  "use strict";

  const mount = document.getElementById("jsspeccy");
  const status = document.getElementById("emulator-status");
  const wideViewport = window.matchMedia("(min-width: 46rem)");

  const setStatus = (state, message) => {
    status.dataset.state = state;
    status.textContent = message;
  };

  if (typeof window.JSSpeccy !== "function") {
    setStatus("error", "Emulator failed to load — use the TAP download.");
    return;
  }

  try {
    const emulator = window.JSSpeccy(mount, {
      machine: 128,
      openUrl: "rogue-zx128.tap",
      autoLoadTapes: true,
      tapeAutoLoadMode: "default",
      sandbox: true,
      zoom: wideViewport.matches ? 2 : 1,
    });

    const keyboardRoot = mount.querySelector("[tabindex='0']");
    if (keyboardRoot) {
      keyboardRoot.setAttribute("role", "application");
      keyboardRoot.setAttribute("aria-label", "ZX Spectrum keyboard input");
      keyboardRoot.setAttribute("aria-describedby", "player-instructions");
      keyboardRoot.style.removeProperty("outline");
    }

    const labelEmulatorControls = () => {
      mount.querySelectorAll("button").forEach((button) => {
        const title = button.getAttribute("title");
        const textLabel = button.textContent.trim();

        if (title) {
          button.setAttribute("aria-label", title);
        } else if (!textLabel && !button.hasAttribute("aria-label")) {
          const isDialogControl = button.closest("[role='dialog'], .dialog");
          button.setAttribute(
            "aria-label",
            isDialogControl ? "Close dialog" : "Start emulator"
          );
        }
      });
    };

    labelEmulatorControls();
    const controlObserver = new MutationObserver(labelEmulatorControls);
    controlObserver.observe(mount, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["title"],
    });

    emulator.onReady(() => {
      setStatus("success", "Ready — press ▶ to start.");
    });

    const syncZoom = (event) => {
      emulator.setZoom(event.matches ? 2 : 1);
    };

    if (typeof wideViewport.addEventListener === "function") {
      wideViewport.addEventListener("change", syncZoom);
    } else {
      wideViewport.addListener(syncZoom);
    }
  } catch (error) {
    setStatus("error", "Emulator failed to initialise — use the TAP download.");
    console.error(error);
  }
})();
