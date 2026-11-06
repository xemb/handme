#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "mongoose.h"

#define UPLOAD_DIR "./uploads"
#define WEB_DIR "./dist"
#define MAX_NAME 255

// Only plain file names are allowed: reject empty names, path separators,
// control characters, and quotes
static bool safe_name(const char *name) {
  size_t n = strlen(name);
  if (n == 0 || n > MAX_NAME)
    return false;
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    return false;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)name[i];
    if (ch < 0x20 || ch == 0x7f || ch == '/' || ch == '\\' || ch == '"') {
      return false;
    }
  }
  return true;
}

static bool io_puts(struct mg_iobuf *io, const char *s) {
  size_t n = strlen(s);
  return mg_iobuf_add(io, io->len, s, n) == n;
}

// GET /files - list files in the uploads directory
static void list_files(struct mg_connection *c) {
  DIR *dir = opendir(UPLOAD_DIR);
  if (dir == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"cannot open uploads dir\"}\n");
    return;
  }

  struct mg_iobuf io = {0};
  bool first = true, ok = io_puts(&io, "{\"ok\":true,\"files\":[");

  struct dirent *de;
  while (ok && (de = readdir(dir)) != NULL) {
    char path[512];
    struct stat st;
    if (!safe_name(de->d_name))
      continue;
    mg_snprintf(path, sizeof(path), UPLOAD_DIR "/%s", de->d_name);
    if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode))
      continue;

    char *item = mg_mprintf("%s{\"name\":%m,\"size\":%lld}", first ? "" : ",",
                            MG_ESC(de->d_name), (long long)st.st_size);
    if (item == NULL) {
      ok = false;
    } else {
      ok = io_puts(&io, item);
      mg_free(item);
      first = false;
    }
  }
  closedir(dir);

  if (ok)
    ok = io_puts(&io, "]}");
  if (ok) {
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%.*s\n",
                  (int)io.len, (char *)io.buf);
  } else {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"out of memory\"}\n");
  }
  mg_iobuf_free(&io);
}

// GET /download?file=<name> - download a file as an attachment
static void download_file(struct mg_connection *c, struct mg_http_message *hm) {
  char name[256];
  int n = mg_http_get_var(&hm->query, "file", name, sizeof(name));
  if (n <= 0 || !safe_name(name)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"bad filename\"}\n");
    return;
  }

  char path[512];
  mg_snprintf(path, sizeof(path), UPLOAD_DIR "/%s", name);

  struct stat st;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"file not found\"}\n");
    return;
  }

  // Use an ASCII fallback for filename and RFC 5987 filename* for the full
  // (possibly non-ASCII) name
  char fallback[256], encoded[1024], headers[1400];
  size_t j = 0;
  for (size_t i = 0; name[i] != '\0' && j + 1 < sizeof(fallback); i++) {
    unsigned char ch = (unsigned char)name[i];
    bool keep = ch >= 0x20 && ch < 0x7f && ch != '"' && ch != '\\';
    fallback[j++] = keep ? (char)ch : '_';
  }
  fallback[j] = '\0';
  mg_url_encode(name, strlen(name), encoded, sizeof(encoded));
  mg_snprintf(headers, sizeof(headers),
              "Content-Disposition: attachment; filename=\"%s\"; "
              "filename*=UTF-8''%s\r\n"
              "Cache-Control: no-store\r\n",
              fallback, encoded);

  struct mg_http_serve_opts opts = {.extra_headers = headers};
  mg_http_serve_file(c, hm, path, &opts);
}

// DELETE /delete?file=<name> - delete a file from the uploads directory
static void delete_file(struct mg_connection *c, struct mg_http_message *hm) {
  char name[256];
  int n = mg_http_get_var(&hm->query, "file", name, sizeof(name));
  if (n <= 0 || !safe_name(name)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"bad filename\"}\n");
    return;
  }

  char path[512];
  mg_snprintf(path, sizeof(path), UPLOAD_DIR "/%s", name);

  struct stat st;
  if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"file not found\"}\n");
    return;
  }

  if (remove(path) != 0) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"delete failed\"}\n");
    return;
  }

  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"ok\":true,\"name\":%m}\n", MG_ESC(name));
}

// Save a file from a multipart request. Returns bytes written,
// -1 = save failed, -2 = invalid filename, -3 = no file field
static long save_upload(struct mg_http_message *hm, char *out_name,
                        size_t out_size) {
  char query_name[256] = {0};
  int qlen =
      mg_http_get_var(&hm->query, "file", query_name, sizeof(query_name));
  size_t ofs = 0;
  struct mg_http_part part;

  while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
    char name[256];
    if (part.filename.len == 0)
      continue;

    if (qlen > 0) {
      mg_snprintf(name, sizeof(name), "%s", query_name);
    } else {
      // Strip any path components from client-provided filenames
      size_t start = 0;
      for (size_t i = 0; i < part.filename.len; i++) {
        if (part.filename.buf[i] == '/' || part.filename.buf[i] == '\\') {
          start = i + 1;
        }
      }
      size_t len = part.filename.len - start;
      if (len >= sizeof(name))
        len = sizeof(name) - 1;
      memcpy(name, part.filename.buf + start, len);
      name[len] = '\0';
    }

    if (!safe_name(name))
      return -2;

    char path[512];
    mg_snprintf(path, sizeof(path), UPLOAD_DIR "/%s", name);
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
      return -1;
    size_t written =
        part.body.len == 0 ? 0 : fwrite(part.body.buf, 1, part.body.len, fp);
    if (fclose(fp) != 0 || written != part.body.len) {
      remove(path);
      return -1;
    }

    mg_snprintf(out_name, out_size, "%s", name);
    return (long)written;
  }

  return -3;
}

// POST /upload?file=<name> - upload a file
static void upload_file(struct mg_connection *c, struct mg_http_message *hm) {
  char name[256] = {0};
  long written = save_upload(hm, name, sizeof(name));

  if (written == -2) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"bad filename\"}\n");
  } else if (written == -3) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"no file\"}\n");
  } else if (written < 0) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{\"ok\":false,\"error\":\"save failed\"}\n");
  } else {
    mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                  "{\"ok\":true,\"name\":%m,\"size\":%ld}\n", MG_ESC(name),
                  written);
  }
}

// In production, serve the built frontend from the same server
static void serve_web(struct mg_connection *c, struct mg_http_message *hm) {
  struct stat st;
  if (stat(WEB_DIR "/index.html", &st) == 0) {
    struct mg_http_serve_opts opts = {.root_dir = WEB_DIR};
    mg_http_serve_dir(c, hm, &opts);
  } else {
    mg_http_reply(c, 404, "Content-Type: text/plain; charset=utf-8\r\n",
                  "The frontend has not been built. "
                  "Run \"make dev\" for development or \"make start\" "
                  "for production.\n");
  }
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev != MG_EV_HTTP_MSG)
    return;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;

  if (mg_match(hm->uri, mg_str("/upload"), NULL)) {
    if (mg_strcasecmp(hm->method, mg_str("POST")) != 0) {
      mg_http_reply(c, 405, "Allow: POST\r\n", "method not allowed\n");
    } else {
      upload_file(c, hm);
    }
  } else if (mg_match(hm->uri, mg_str("/files"), NULL)) {
    list_files(c);
  } else if (mg_match(hm->uri, mg_str("/download"), NULL)) {
    download_file(c, hm);
  } else if (mg_match(hm->uri, mg_str("/delete"), NULL)) {
    if (mg_strcasecmp(hm->method, mg_str("DELETE")) != 0) {
      mg_http_reply(c, 405, "Allow: DELETE\r\n", "method not allowed\n");
    } else {
      delete_file(c, hm);
    }
  } else {
    serve_web(c, hm);
  }
}

int main(void) {
  mkdir(UPLOAD_DIR, 0755);
  mg_log_set(MG_LL_ERROR);

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  const char *url = "http://0.0.0.0:8080";
  if (mg_http_listen(&mgr, url, fn, NULL) == NULL) {
    fprintf(stderr, "cannot listen on %s\n", url);
    mg_mgr_free(&mgr);
    return 1;
  }

  printf("handme server listening on %s\n", url);
  fflush(stdout);
  for (;;)
    mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  return 0;
}
