"use strict";

const fileInput = document.getElementById("file-input");
const dropzone = document.getElementById("dropzone");
const fileList = document.getElementById("file-list");
const emptyState = document.getElementById("empty");
const loading = document.getElementById("loading");
const fileCount = document.getElementById("file-count");
const refreshBtn = document.getElementById("refresh");
const progressWrap = document.getElementById("progress-wrap");
const progressBar = document.getElementById("progress-bar");
const progressName = document.getElementById("progress-name");
const progressPct = document.getElementById("progress-pct");
const toast = document.getElementById("toast");

let toastTimer = null;
let uploading = false;

function showToast(message, kind) {
  toast.textContent = message;
  toast.className = "toast show" + (kind ? " " + kind : "");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.className = "toast";
  }, 3200);
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + " B";
  const units = ["KB", "MB", "GB", "TB"];
  let value = bytes / 1024;
  let i = 0;
  while (value >= 1024 && i < units.length - 1) {
    value /= 1024;
    i++;
  }
  return (value >= 10 ? value.toFixed(0) : value.toFixed(1)) + " " + units[i];
}

function fileIcon() {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("aria-hidden", "true");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute(
    "d",
    "M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8l-5-5Zm0 0v5h5"
  );
  path.setAttribute("fill", "none");
  path.setAttribute("stroke", "currentColor");
  path.setAttribute("stroke-width", "1.8");
  path.setAttribute("stroke-linejoin", "round");
  svg.appendChild(path);
  return svg;
}

function actionButton(label, href, className) {
  const isLink = Boolean(href);
  const el = document.createElement(isLink ? "a" : "button");
  el.className = "btn" + (className ? " " + className : "");
  if (isLink) {
    el.href = href;
    el.setAttribute("download", "");
  } else {
    el.type = "button";
  }
  const text = document.createElement("span");
  text.textContent = label;
  el.appendChild(text);
  return el;
}

function renderFiles(files) {
  fileList.replaceChildren();
  fileCount.textContent = files.length ? "(" + files.length + ")" : "";
  emptyState.hidden = files.length > 0;
  loading.hidden = true;

  for (const file of files) {
    const tr = document.createElement("tr");

    const nameCell = document.createElement("td");
    const nameWrap = document.createElement("div");
    nameWrap.className = "file-name";
    nameWrap.appendChild(fileIcon());
    const name = document.createElement("span");
    name.textContent = file.name;
    name.title = file.name;
    nameWrap.appendChild(name);
    nameCell.appendChild(nameWrap);

    const sizeCell = document.createElement("td");
    sizeCell.className = "size";
    sizeCell.textContent = formatSize(file.size);

    const actionsCell = document.createElement("td");
    actionsCell.className = "actions";
    const download = actionButton(
      "Download",
      "/download?file=" + encodeURIComponent(file.name)
    );
    const del = actionButton("Delete", null, "danger");
    del.addEventListener("click", () => deleteFile(file.name, del));
    actionsCell.append(download, del);

    tr.append(nameCell, sizeCell, actionsCell);
    fileList.appendChild(tr);
  }

  if (!files.length) {
    emptyState.hidden = false;
  }
}

async function loadFiles() {
  refreshBtn.disabled = true;
  try {
    const res = await fetch("/files", { cache: "no-store" });
    const data = await res.json();
    if (!res.ok || !data.ok) {
      throw new Error(data.error || "Could not load files");
    }
    renderFiles(Array.isArray(data.files) ? data.files : []);
  } catch (err) {
    loading.hidden = true;
    fileList.replaceChildren();
    emptyState.hidden = false;
    emptyState.textContent = "Could not load files. Try refreshing.";
    showToast(err.message || "Could not load files", "error");
  } finally {
    refreshBtn.disabled = false;
  }
}

function uploadFile(file) {
  return new Promise((resolve, reject) => {
    const form = new FormData();
    form.append("file", file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/upload");
    xhr.responseType = "json";

    xhr.upload.addEventListener("progress", (e) => {
      if (!e.lengthComputable) return;
      const pct = Math.round((e.loaded / e.total) * 100);
      progressBar.style.width = pct + "%";
      progressPct.textContent = pct + "%";
    });

    xhr.addEventListener("load", () => {
      let data = xhr.response;
      if (!data) {
        try {
          data = JSON.parse(xhr.responseText);
        } catch {
          data = null;
        }
      }
      if (xhr.status >= 200 && xhr.status < 300 && data && data.ok) {
        resolve(data);
      } else {
        reject(new Error((data && data.error) || "Upload failed"));
      }
    });

    xhr.addEventListener("error", () => reject(new Error("Network error")));
    xhr.addEventListener("abort", () => reject(new Error("Upload cancelled")));
    xhr.send(form);
  });
}

async function uploadFiles(fileListLike) {
  const files = Array.from(fileListLike);
  if (!files.length || uploading) return;

  uploading = true;
  dropzone.classList.remove("dragover");
  progressWrap.hidden = false;

  let succeeded = 0;
  for (const file of files) {
    progressName.textContent = file.name;
    progressBar.style.width = "0%";
    progressPct.textContent = "0%";
    try {
      await uploadFile(file);
      succeeded++;
    } catch (err) {
      showToast(file.name + ": " + err.message, "error");
    }
  }

  uploading = false;
  progressWrap.hidden = true;
  await loadFiles();
  if (succeeded > 0) {
    showToast(
      succeeded === 1 ? "File uploaded" : succeeded + " files uploaded",
      "success"
    );
  }
}

async function deleteFile(name, button) {
  if (!window.confirm('Delete "' + name + '"?')) return;
  button.disabled = true;
  try {
    const res = await fetch("/delete?file=" + encodeURIComponent(name), {
      method: "DELETE",
    });
    const data = await res.json();
    if (!res.ok || !data.ok) {
      throw new Error(data.error || "Delete failed");
    }
    showToast("Deleted " + name, "success");
    await loadFiles();
  } catch (err) {
    button.disabled = false;
    showToast(err.message || "Delete failed", "error");
  }
}

dropzone.addEventListener("click", () => fileInput.click());
dropzone.addEventListener("keydown", (e) => {
  if (e.key === "Enter" || e.key === " ") {
    e.preventDefault();
    fileInput.click();
  }
});

fileInput.addEventListener("change", () => {
  uploadFiles(fileInput.files);
  fileInput.value = "";
});

for (const type of ["dragenter", "dragover"]) {
  dropzone.addEventListener(type, (e) => {
    e.preventDefault();
    dropzone.classList.add("dragover");
  });
}

for (const type of ["dragleave", "drop"]) {
  dropzone.addEventListener(type, (e) => {
    e.preventDefault();
    if (type === "dragleave" && dropzone.contains(e.relatedTarget)) return;
    dropzone.classList.remove("dragover");
  });
}

dropzone.addEventListener("drop", (e) => {
  if (e.dataTransfer && e.dataTransfer.files.length) {
    uploadFiles(e.dataTransfer.files);
  }
});

refreshBtn.addEventListener("click", loadFiles);

loadFiles();
