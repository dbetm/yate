import json
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, messagebox, font
from typing import Any, Dict, List, Optional



class GUI:
    DEFAULT_WIDTH = 950
    DEFAULT_HEIGHT = 460

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("YATE")
        self.root.minsize(width=self.DEFAULT_WIDTH, height=self.DEFAULT_HEIGHT)
        self.root.attributes('-alpha', 0.9)
        # handle closing the window
        self.root.protocol("WM_DELETE_WINDOW", self.quit)

        default_font = font.Font(family="Comic Sans MS", size=14)
        self.content = tk.Text(
            self.root, wrap="word", padx=10, pady=10, font=default_font, spacing3=4
        )
        self.content.pack(expand=True, fill="both") # add to the root (parent)

        self.menubar = tk.Menu(self.root)
        self.filemenu = tk.Menu(self.menubar)
        self.emojimenu = tk.Menu(self.filemenu, tearoff=0)
        self.was_modified = False

        # Add status bar
        self.status_bar = tk.Label(
            self.root,
            text=" Ready",
            bd=1, # border width
            relief=tk.SUNKEN, # 3D effect
            anchor=tk.W, # align text to the left 
        )

        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)

    def _on_text_change(self):
        # IMPORTANTE: resetear el flag de modified
        self.content.edit_modified(False)

        if not self.was_modified:
            self.was_modified = True

        self.update_status(len(self.content.get("1.0", "end-1c")))

    def run(self):
        self.root.mainloop()

    def quit(self):
        confirm_msg = "Are you sure you want to exit?"
        if self.was_modified:
            if not messagebox.askokcancel("Changes unsaved", message=confirm_msg, icon="warning"):
                return

        self.root.quit()

    def confirm_open_new_file(self) -> bool:
        """Open a message box asking to confirm that the user wants to open a new file if there are 
        unsaved changes."""
        confirm_msg = "Are you sure you open another file without saving this?"
        if self.was_modified:
            return messagebox.askokcancel("Changes unsaved - new file?", message=confirm_msg)

        return True

    def reset_modified(self):
        """Set to `False` the flag to know if there were any modification"""
        self.was_modified = False

    def update_status(self, total_chars: int):
        """Update status bar with total characters"""
        total_chars_str = "characters"

        if total_chars == 1:
            total_chars_str = "character"

        modified_status = " | modified" if self.was_modified else "" 

        self.status_bar.config(text=f" {total_chars} {total_chars_str}{modified_status}")


class FileEditor:
    BEGIN_FILE = "0.0" # 0 row and 0 column
    ASSETS_PATH = Path("assets")
    METADATA_PATH = ASSETS_PATH / "metadata.json"
    EMOJIS_PATH = ASSETS_PATH / "emojis.txt"
    ENCODING = "utf-8"

    def __init__(self, gui: GUI):
        self.gui = gui
        self.metadata: Dict[str, Any] = self._load_metadata()
        self.filepath: Optional[Path] = None

        emojis = self._load_emojis()
        self.__configure_menu(emojis)

    def __configure_menu(self, emojis: List[str]) -> None:
        self.gui.filemenu.add_command(label="New", command=self.new_file, accelerator="Ctrl+N")
        self.gui.filemenu.add_command(label="Open", command=self.open_file, accelerator="Ctrl+O")
        self.gui.filemenu.add_command(label="Save", command=self.save_file, accelerator="Ctrl+S")
        self.gui.filemenu.add_command(label="Save As", command=self.save_as, accelerator="Ctrl+w")
        self.gui.filemenu.add_separator()

        # Insert Emoji submenu
        for emoji in emojis:
            self.gui.emojimenu.add_command(label=emoji, command=lambda e = emoji: self._insert_emoji(e))
        self.gui.filemenu.add_cascade(label="Insert emoji", menu=self.gui.emojimenu)

        self.gui.filemenu.add_separator()
        self.gui.filemenu.add_command(label="Quit", command=self.gui.quit, accelerator="Ctrl+Q")

        self.gui.menubar.add_cascade(label="File", menu=self.gui.filemenu)
        self.gui.root.config(menu=self.gui.menubar)

        # Shortcuts
        bindings = {
            "<Control-n>": self.new_file,
            "<Control-o>": self.open_file,
            "<Control-s>": self.save_file,
            "<Control-w>": self.save_as,
            "<Control-q>": self.gui.quit,
            "<<Modified>>": self.gui._on_text_change,
        }
        for key, func in bindings.items():
            self.gui.root.bind_all(key, lambda _, f=func: f())

    def _load_metadata(self) -> dict:
        """Load metadata JSON file to load data related to past user's interactions."""
        if self.METADATA_PATH.exists():
            with open(self.METADATA_PATH, "r", encoding=self.ENCODING) as file:
                return json.load(file)

        print("Warning: Metadata file not found")
        return {}

    def _load_emojis(self) -> List[str]:
        if self.EMOJIS_PATH.exists():
            with open(self.EMOJIS_PATH, "r", encoding=self.ENCODING) as f:
                return [line.strip() for line in f if line.strip()]

        print("Warning: Emojis file not found")
        return []

    def _insert_emoji(self, emoji: str) -> None:
        self.gui.content.insert(tk.INSERT, emoji)

    def _update_metadata(self, **kwargs) -> None:
        """Update in-memory metadata dictionary and save it into disk."""
        self.metadata.update(kwargs)

        with open(self.METADATA_PATH, "w", encoding=self.ENCODING) as f:
            json.dump(self.metadata, f, indent=4)

        print("Metadata was updated...")

    def _update_title(self):
        if self.filepath:
            filename = self.filepath.name
        else:
            filename = "Untitled"
        self.gui.root.title(f"YATE - {filename}")

    def _generate_new_filepath(self, directory: Path) -> Path:
        """Return a filepath for new files with a default name and timestamp."""
        datetime_str = datetime.now().strftime("%Y%m%d-%H%M%S")
        return directory / f"untitled-{datetime_str}.txt"

    def new_file(self):
        self.filepath = None
        self._update_title()
        # Delete all the current content
        self.gui.content.delete(self.BEGIN_FILE, tk.END)

    def save_file(self):
        """Save new file or new changes of a opened file."""
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        if not self.filepath:
            directory = filedialog.askdirectory(
                initialdir=self.metadata.get("last_path_used", "~/")
            )

            if not directory:
                return

            self.filepath = self._generate_new_filepath(Path(directory))

        with open(self.filepath, "w") as f:
            f.write(text)

        self._update_title()
        self._update_metadata(last_path_used=str(self.filepath.parent))
        self.gui.reset_modified()

    def save_as(self):
        """Save file as a copy, maybe a different path with another name which will chose by the user."""
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        # Get dialog, the future request will remember the chosen directory
        filepath = filedialog.asksaveasfilename(defaultextension=".txt")

        if not filepath:
            return

        self.filepath = Path(filepath)

        try:
            with open(self.filepath, "w") as f:
                f.write(text)
        except OSError:
            messagebox.showerror(title="Oops!", message="Unable to save file...")
        
        self._update_title()
        self._update_metadata(last_path_used=str(self.filepath.parent))
        self.gui.reset_modified()

    def open_file(self):
        if not self.gui.confirm_open_new_file():
            return

        filepath = filedialog.askopenfilename(
            defaultextension=".txt",
            filetypes=[("Text Files", "*.txt")],
            initialdir=self.metadata.get("last_path_used", "~/")
        )

        if not filepath:
            return

        self.filepath = Path(filepath)

        with open(self.filepath, "r", encoding=self.ENCODING) as f:
            content = f.read()

        self._update_title()
        self.gui.update_status(len(content))

        self.gui.content.delete(self.BEGIN_FILE, tk.END)
        self.gui.content.insert(self.BEGIN_FILE, content)

        self._update_metadata(last_path_used="/".join(filepath.split("/")[:-1]))
        self.gui.reset_modified()



if __name__ == "__main__":
    gui = GUI()
    editor = FileEditor(gui)

    gui.run()
