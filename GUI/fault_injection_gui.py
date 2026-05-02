"""
fault_injection_gui.py
EB Tresos-style PyQt6 GUI for the Fault Injection Framework
Replaces the C CLI tool — reads/writes fault_config.json & test_cases.json
"""

import sys
import json
import os
from datetime import datetime
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QDockWidget, QDialog,
    QTreeWidget, QTreeWidgetItem, QTabWidget, QTableWidget, QTableWidgetItem,
    QFormLayout, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QLineEdit, QComboBox, QCheckBox, QPushButton, QSpinBox,
    QListWidget, QListWidgetItem, QScrollArea, QFrame, QGroupBox,
    QToolBar, QStatusBar, QHeaderView, QMenuBar, QMenu,
    QMessageBox, QAbstractItemView, QSplitter, QSizePolicy, QTextEdit
)
from PyQt6.QtCore import Qt, QSize, QSettings, QTimer
from PyQt6.QtGui import QAction, QFont, QColor, QIcon, QKeySequence

# ─────────────────────────────────────────────────────────────────────────────
#  CONSTANTS
# ─────────────────────────────────────────────────────────────────────────────

MAX_FAULTS          = 10
MAX_TEST_CASES      = 20
MAX_FAULTS_PER_TEST = 10

FAULT_TYPES = {
    0:  "NONE",
    1:  "BIT_FLIP",
    2:  "MULTI_BIT_FLIP",
    3:  "STUCK_AT_0",
    4:  "STUCK_AT_1",
    5:  "DELAY",
    6:  "OMISSION",
    7:  "DATA_CORRUPTION",
    8:  "CRC_CORRUPTION",
    9:  "RETURN_VALUE_CORRUPTION",
    10: "PARAMETER_CORRUPTION",
    11: "QUEUE_OVERFLOW",
    12: "PARTIAL_ERASE",
}

TARGET_MODULES = {
    20:  "NvM (20)",
    21:  "Fee (21)",
    22:  "MemIf (22)",
    90:  "Eep (90)",
    92:  "Fls (92)",
    212: "EA (212)",
}

EXPECTED_RESULTS = {
    0: "DATA_CORRUPTED",
    1: "DATA_CLEAN",
    2: "MANUAL",
}

# Fault type categories
BITPOS_TYPES        = {1, 3, 4}         # BIT_FLIP, STUCK_AT_0, STUCK_AT_1
MASK_TYPES          = {2}               # MULTI_BIT_FLIP uses Mask byte
PARTIAL_ERASE_TYPES = {12}              # PARTIAL_ERASE uses MaxEraseBytes
NEITHER_TYPES       = {0, 5, 6, 7, 8, 9, 10, 11}

# Service IDs per module — format: 0x[ModuleID_Hex][ServiceID_Hex] (FaultInjection_Types.h)
SERVICE_IDS = {
    20: {   # NvM
        0x1400: "NVM_INIT",
        0x1401: "NVM_SET_DATA_INDEX",
        0x1404: "NVM_GET_ERROR_STATUS",
        0x1406: "NVM_READ_BLOCK",
        0x1407: "NVM_WRITE_BLOCK",
        0x140C: "NVM_READ_ALL",
        0x140D: "NVM_WRITE_ALL",
        0x140F: "NVM_INVALIDATE_NV_BLOCK",
        0x1410: "NVM_ERASE_NV_BLOCK",
        0x1411: "NVM_MAIN_FUNCTION",
    },
    21: {   # Fee
        0x1500: "FEE_INIT",
        0x1502: "FEE_READ",
        0x1503: "FEE_WRITE",
        0x1505: "FEE_GET_STATUS",
        0x1506: "FEE_GET_JOB_RESULT",
        0x1507: "FEE_INVALIDATE_BLOCK",
        0x1512: "FEE_MAIN_FUNCTION",
    },
    22: {   # MemIf
        0x1602: "MEMIF_READ",
        0x1603: "MEMIF_WRITE",
        0x1605: "MEMIF_GET_STATUS",
        0x1606: "MEMIF_GET_JOB_RESULT",
        0x1607: "MEMIF_INVALIDATE_BLOCK",
        0x1608: "MEMIF_ERASE_IMM_BLOCK",
    },
    92: {   # Fls
        0x5C00: "FLS_INIT",
        0x5C01: "FLS_ERASE",
        0x5C02: "FLS_WRITE",
        0x5C03: "FLS_READ",
        0x5C05: "FLS_GET_STATUS",
        0x5C06: "FLS_GET_JOB_RESULT",
        0x5CFF: "FLS_MAIN_FUNCTION",
    },
    90:  {},   # Eep  — no service IDs defined in project
    212: {},   # EA   — no service IDs defined in project
}

def service_id_label(combined_id: int) -> str:
    module_id = (combined_id >> 8) & 0xFF
    return SERVICE_IDS.get(module_id, {}).get(combined_id, f"0x{combined_id:04X}")

def module_short(mid: int) -> str:
    return TARGET_MODULES.get(mid, str(mid)).split(" (")[0]

def fault_target_label(f: dict) -> str:
    mid = f.get("TargetModuleID", 0)
    sid = f.get("TargetModuleServiceID", 0)
    mod = module_short(mid)
    svc = service_id_label(sid) if sid else ""
    return f"{mod} › {svc}" if svc else mod

SCRIPT_DIR    = os.path.dirname(os.path.abspath(__file__))
FAULT_JSON    = os.path.join(SCRIPT_DIR, "fault_config.json")
TEST_JSON     = os.path.join(SCRIPT_DIR, "test_cases.json")
SETTINGS_JSON = os.path.join(SCRIPT_DIR, "app_settings.json")

# ─────────────────────────────────────────────────────────────────────────────
#  STYLESHEETS
# ─────────────────────────────────────────────────────────────────────────────

DARK_STYLE = """
QMainWindow, QWidget {
    background-color: #3C3F41;
    color: #BBBBBB;
    font-family: 'Segoe UI', sans-serif;
    font-size: 12px;
}
QMenuBar {
    background-color: #2B2B2B;
    color: #BBBBBB;
    border-bottom: 1px solid #555;
}
QMenuBar::item:selected  { background-color: #4B6EAF; }
QMenu {
    background-color: #3C3F41;
    border: 1px solid #555;
    color: #BBBBBB;
}
QMenu::item:selected { background-color: #4B6EAF; color: #FFFFFF; }
QMenu::separator { height: 1px; background: #555; margin: 2px 0; }

QToolBar {
    background-color: #3C3F41;
    border-bottom: 1px solid #555;
    spacing: 3px;
    padding: 3px;
}
QToolBar::separator { background: #555; width: 1px; margin: 4px 2px; }
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 3px 7px;
    color: #BBBBBB;
    font-size: 12px;
}
QToolButton:hover  { background-color: #4C5052; border-color: #666; }
QToolButton:pressed { background-color: #2D5DA8; }

QDockWidget {
    color: #BBBBBB;
    font-weight: bold;
    font-size: 11px;
}
QDockWidget::title {
    background-color: #2B2B2B;
    border: 1px solid #555;
    padding: 5px 8px;
    text-align: left;
}
QDockWidget::close-button, QDockWidget::float-button {
    border: none;
    background: transparent;
}

QTreeWidget {
    background-color: #2B2B2B;
    border: none;
    color: #BBBBBB;
    alternate-background-color: #303030;
    show-decoration-selected: 1;
}
QTreeWidget::item:hover    { background-color: #3D3D3D; }
QTreeWidget::item:selected { background-color: #2D5DA8; color: #FFFFFF; }

QTableWidget {
    background-color: #2B2B2B;
    gridline-color: #3A3A3A;
    border: none;
    color: #BBBBBB;
    alternate-background-color: #303030;
    selection-background-color: #2D5DA8;
    selection-color: #FFFFFF;
}
QTableWidget::item:hover { background-color: #35383A; }
QTableCornerButton::section {
    background-color: #3C3F41;
    border: 1px solid #555;
}
QHeaderView::section {
    background-color: #3C3F41;
    border: 1px solid #444;
    border-left: none;
    padding: 5px 8px;
    color: #AAAAAA;
    font-weight: bold;
    font-size: 11px;
}
QHeaderView::section:first { border-left: 1px solid #444; }

QTabWidget::pane {
    border: 1px solid #555;
    background-color: #3C3F41;
    top: -1px;
}
QTabBar::tab {
    background-color: #3C3F41;
    border: 1px solid #555;
    border-bottom: none;
    padding: 5px 14px;
    color: #AAAAAA;
    margin-right: 2px;
    font-size: 11px;
}
QTabBar::tab:selected {
    background-color: #2B2B2B;
    color: #FFFFFF;
    border-top: 2px solid #4B6EAF;
}
QTabBar::tab:hover:!selected { background-color: #4C5052; }

QLineEdit, QSpinBox, QComboBox, QTextEdit {
    background-color: #2B2B2B;
    border: 1px solid #555;
    border-radius: 2px;
    padding: 4px 7px;
    color: #BBBBBB;
    min-height: 22px;
    selection-background-color: #4B6EAF;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus {
    border-color: #4B6EAF;
    background-color: #252525;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    color: #666;
    background-color: #333;
}
QSpinBox::up-button, QSpinBox::down-button {
    background-color: #3C3F41;
    border: none;
    width: 16px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
    background-color: #4B6EAF;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
    background-color: #3C3F41;
}
QComboBox::down-arrow { color: #AAAAAA; }
QComboBox QAbstractItemView {
    background-color: #3C3F41;
    selection-background-color: #4B6EAF;
    border: 1px solid #555;
    color: #BBBBBB;
}

QCheckBox { spacing: 6px; color: #BBBBBB; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #777;
    background-color: #2B2B2B;
    border-radius: 2px;
}
QCheckBox::indicator:checked {
    background-color: #4B6EAF;
    border-color: #4B6EAF;
}
QCheckBox::indicator:hover { border-color: #4B6EAF; }

QGroupBox {
    border: 1px solid #4B6EAF;
    border-radius: 3px;
    margin-top: 10px;
    padding-top: 10px;
    color: #BBBBBB;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
    color: #4B6EAF;
    font-size: 11px;
}

QListWidget {
    background-color: #2B2B2B;
    border: 1px solid #555;
    color: #BBBBBB;
    alternate-background-color: #303030;
}
QListWidget::item:hover    { background-color: #3D3D3D; }
QListWidget::item:selected { background-color: #2D5DA8; color: #FFFFFF; }

QPushButton {
    background-color: #4C5052;
    border: 1px solid #666;
    border-radius: 3px;
    padding: 5px 14px;
    color: #BBBBBB;
    font-size: 12px;
}
QPushButton:hover  { background-color: #5C6062; border-color: #888; }
QPushButton:pressed { background-color: #4B6EAF; border-color: #4B6EAF; }
QPushButton:disabled { background-color: #3A3A3A; color: #555; border-color: #444; }
QPushButton#btn_primary {
    background-color: #4B6EAF;
    border-color: #4B6EAF;
    color: #FFFFFF;
    font-weight: bold;
}
QPushButton#btn_primary:hover  { background-color: #5C7DC0; }
QPushButton#btn_danger {
    background-color: #7A1E1E;
    border-color: #AA2222;
    color: #FFAAAA;
}
QPushButton#btn_danger:hover { background-color: #992222; }

QScrollBar:vertical {
    background: #2B2B2B; width: 10px; margin: 0;
}
QScrollBar::handle:vertical {
    background: #555; min-height: 24px; border-radius: 5px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #2B2B2B; height: 10px;
}
QScrollBar::handle:horizontal {
    background: #555; min-width: 24px; border-radius: 5px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QStatusBar {
    background-color: #2B2B2B;
    border-top: 1px solid #4B6EAF;
    color: #AAAAAA;
    font-size: 11px;
    padding: 0 6px;
}
QStatusBar::item { border: none; }

QSplitter::handle { background-color: #555; }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical   { height: 3px; }

QDialog {
    background-color: #3C3F41;
    color: #BBBBBB;
}
QLabel { color: #BBBBBB; }
QLabel#section_label {
    color: #4B6EAF;
    font-weight: bold;
    font-size: 11px;
    border-bottom: 1px solid #4B6EAF;
    padding-bottom: 2px;
}
QLabel#hint_label {
    color: #777777;
    font-size: 11px;
    font-style: italic;
}
"""

LIGHT_STYLE = """
QMainWindow, QWidget {
    background-color: #F5F5F5;
    color: #1E1E1E;
    font-family: 'Segoe UI', sans-serif;
    font-size: 12px;
}
QMenuBar {
    background-color: #ECECEC;
    color: #1E1E1E;
    border-bottom: 1px solid #CCCCCC;
}
QMenuBar::item:selected { background-color: #4B6EAF; color: #FFF; }
QMenu {
    background-color: #FFFFFF;
    border: 1px solid #CCCCCC;
    color: #1E1E1E;
}
QMenu::item:selected { background-color: #4B6EAF; color: #FFF; }
QMenu::separator { height: 1px; background: #CCC; margin: 2px 0; }

QToolBar {
    background-color: #ECECEC;
    border-bottom: 1px solid #CCCCCC;
    spacing: 3px; padding: 3px;
}
QToolBar::separator { background: #CCC; width: 1px; margin: 4px 2px; }
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 3px 7px;
    color: #1E1E1E;
}
QToolButton:hover  { background-color: #DCDCDC; border-color: #AAAAAA; }
QToolButton:pressed { background-color: #4B6EAF; color: #FFF; }

QDockWidget { color: #1E1E1E; font-weight: bold; }
QDockWidget::title {
    background-color: #ECECEC;
    border: 1px solid #CCCCCC;
    padding: 5px 8px;
    text-align: left;
}

QTreeWidget {
    background-color: #FFFFFF;
    border: 1px solid #CCCCCC;
    color: #1E1E1E;
    alternate-background-color: #F9F9F9;
}
QTreeWidget::item:hover    { background-color: #E8F0FF; }
QTreeWidget::item:selected { background-color: #4B6EAF; color: #FFFFFF; }

QTableWidget {
    background-color: #FFFFFF;
    gridline-color: #E0E0E0;
    border: 1px solid #CCCCCC;
    color: #1E1E1E;
    alternate-background-color: #F9F9F9;
    selection-background-color: #4B6EAF;
    selection-color: #FFFFFF;
}
QTableWidget::item:hover { background-color: #E8F0FF; }
QHeaderView::section {
    background-color: #ECECEC;
    border: 1px solid #CCCCCC;
    border-left: none;
    padding: 5px 8px;
    color: #444444;
    font-weight: bold;
    font-size: 11px;
}
QHeaderView::section:first { border-left: 1px solid #CCCCCC; }

QTabWidget::pane {
    border: 1px solid #CCCCCC;
    background-color: #F5F5F5;
    top: -1px;
}
QTabBar::tab {
    background-color: #ECECEC;
    border: 1px solid #CCCCCC;
    border-bottom: none;
    padding: 5px 14px;
    color: #555555;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background-color: #FFFFFF;
    color: #1E1E1E;
    border-top: 2px solid #4B6EAF;
}
QTabBar::tab:hover:!selected { background-color: #DCDCDC; }

QLineEdit, QSpinBox, QComboBox, QTextEdit {
    background-color: #FFFFFF;
    border: 1px solid #CCCCCC;
    border-radius: 2px;
    padding: 4px 7px;
    color: #1E1E1E;
    min-height: 22px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #4B6EAF; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #FFFFFF;
    selection-background-color: #4B6EAF;
    border: 1px solid #CCCCCC;
    color: #1E1E1E;
}

QCheckBox { color: #1E1E1E; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #AAAAAA;
    background-color: #FFFFFF;
    border-radius: 2px;
}
QCheckBox::indicator:checked { background-color: #4B6EAF; border-color: #4B6EAF; }

QGroupBox {
    border: 1px solid #4B6EAF;
    border-radius: 3px;
    margin-top: 10px;
    padding-top: 10px;
    color: #1E1E1E;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
    color: #4B6EAF;
    font-size: 11px;
}

QListWidget {
    background-color: #FFFFFF;
    border: 1px solid #CCCCCC;
    color: #1E1E1E;
}
QListWidget::item:hover    { background-color: #E8F0FF; }
QListWidget::item:selected { background-color: #4B6EAF; color: #FFFFFF; }

QPushButton {
    background-color: #E0E0E0;
    border: 1px solid #AAAAAA;
    border-radius: 3px;
    padding: 5px 14px;
    color: #1E1E1E;
}
QPushButton:hover  { background-color: #CCCCCC; }
QPushButton:pressed { background-color: #4B6EAF; color: #FFF; }
QPushButton:disabled { color: #AAAAAA; background-color: #EEEEEE; }
QPushButton#btn_primary {
    background-color: #4B6EAF; color: #FFF;
    border-color: #4B6EAF; font-weight: bold;
}
QPushButton#btn_primary:hover { background-color: #3A5C9E; }
QPushButton#btn_danger {
    background-color: #FFDDDD; border-color: #CC3333; color: #CC0000;
}
QPushButton#btn_danger:hover { background-color: #FFCCCC; }

QScrollBar:vertical   { background: #F0F0F0; width: 10px; }
QScrollBar::handle:vertical { background: #BBBBBB; min-height: 24px; border-radius: 5px; }
QScrollBar:horizontal { background: #F0F0F0; height: 10px; }
QScrollBar::handle:horizontal { background: #BBBBBB; min-width: 24px; border-radius: 5px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }

QStatusBar {
    background-color: #ECECEC;
    border-top: 1px solid #4B6EAF;
    color: #555555;
    font-size: 11px;
}
QDialog { background-color: #F5F5F5; color: #1E1E1E; }
QLabel { color: #1E1E1E; }
QLabel#section_label {
    color: #4B6EAF; font-weight: bold; font-size: 11px;
    border-bottom: 1px solid #4B6EAF; padding-bottom: 2px;
}
QLabel#hint_label {
    color: #888888;
    font-size: 11px;
    font-style: italic;
}
"""

# ─────────────────────────────────────────────────────────────────────────────
#  DATA LAYER
# ─────────────────────────────────────────────────────────────────────────────

def load_json(path: str, default) -> list:
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return default

def save_json(path: str, data) -> bool:
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        return True
    except Exception:
        return False

def next_free_id(used: set, max_val: int) -> int:
    for i in range(max_val):
        if i not in used:
            return i
    return -1

def load_settings() -> dict:
    return load_json(SETTINGS_JSON, {"theme": "dark", "geometry": None})

def save_settings(settings: dict):
    save_json(SETTINGS_JSON, settings)

# ─────────────────────────────────────────────────────────────────────────────
#  FAULT DIALOG
# ─────────────────────────────────────────────────────────────────────────────

class FaultDialog(QDialog):
    def __init__(self, parent=None, fault: dict = None, used_ids: set = None):
        super().__init__(parent)
        self.fault    = fault or {}
        self.used_ids = used_ids or set()
        self.setWindowTitle("Edit Fault" if fault else "Add Fault")
        self.setMinimumWidth(500)
        self.setMinimumHeight(480)
        self.setModal(True)
        self._build_ui()
        if fault:
            self._populate(fault)
        self._update_bit_fields()

    def _build_ui(self):
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 12)
        outer.setSpacing(0)

        # Scrollable content area
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        content = QWidget()
        main_layout = QVBoxLayout(content)
        main_layout.setSpacing(12)
        main_layout.setContentsMargins(16, 16, 16, 8)

        # ── Identity ──
        id_grp  = QGroupBox("Fault Identity")
        id_form = QFormLayout(id_grp)
        id_form.setSpacing(8)

        self.fault_id_lbl = QLabel("(auto-assigned)")
        self.fault_id_lbl.setStyleSheet("color:#4B6EAF; font-weight:bold;")
        id_form.addRow("Fault ID:", self.fault_id_lbl)

        self.active_cb = QCheckBox("Active")
        self.active_cb.setChecked(True)
        id_form.addRow("", self.active_cb)
        main_layout.addWidget(id_grp)

        # ── Target ──
        target_grp  = QGroupBox("Target Configuration")
        target_form = QFormLayout(target_grp)
        target_form.setSpacing(8)

        self.module_cb = QComboBox()
        for mid, name in TARGET_MODULES.items():
            self.module_cb.addItem(name, mid)
        self.module_cb.currentIndexChanged.connect(self._update_service_ids)
        target_form.addRow("Target Module:", self.module_cb)

        self.service_cb = QComboBox()
        target_form.addRow("Service ID Target:", self.service_cb)

        self.type_cb = QComboBox()
        for ftype, name in FAULT_TYPES.items():
            self.type_cb.addItem(name, ftype)
        self.type_cb.currentIndexChanged.connect(self._update_bit_fields)
        target_form.addRow("Fault Type:", self.type_cb)

        # BitPosition — byte selector + bit selector
        self.bitpos_row_lbl = QLabel("Bit Position:")
        self.bitpos_widget  = QWidget()
        bp_layout = QHBoxLayout(self.bitpos_widget)
        bp_layout.setContentsMargins(0, 0, 0, 0)
        bp_layout.setSpacing(6)
        self.byte_spin = QSpinBox()
        self.byte_spin.setRange(0, 255)
        self.byte_spin.setPrefix("Byte ")
        self.byte_spin.setToolTip("Which byte in the buffer to target (0 = first byte)")
        self.bit_spin  = QSpinBox()
        self.bit_spin.setRange(0, 7)
        self.bit_spin.setPrefix("Bit ")
        self.bit_spin.setToolTip("Which bit within that byte (0 = LSB, 7 = MSB)")
        bp_layout.addWidget(self.byte_spin)
        bp_layout.addWidget(QLabel("·"))
        bp_layout.addWidget(self.bit_spin)
        bp_layout.addStretch()
        target_form.addRow(self.bitpos_row_lbl, self.bitpos_widget)

        # Mask row (multi-bit)
        self.mask_row_lbl = QLabel("Mask (hex, e.g. 0xFF):")
        self.mask_edit    = QLineEdit("0xFF")
        self.mask_edit.setPlaceholderText("0x00–0xFF")
        target_form.addRow(self.mask_row_lbl, self.mask_edit)

        # MaxEraseBytes row (partial erase)
        self.max_erase_row_lbl = QLabel("Max Erase Bytes:")
        self.max_erase_spin    = QSpinBox()
        self.max_erase_spin.setRange(1, 65535)
        self.max_erase_spin.setValue(512)
        target_form.addRow(self.max_erase_row_lbl, self.max_erase_spin)

        self.max_erase_hint = QLabel("Must be < the erase length requested at runtime to have effect.")
        self.max_erase_hint.setObjectName("hint_label")
        self.max_erase_hint.setWordWrap(True)
        target_form.addRow("", self.max_erase_hint)

        self._update_service_ids()
        main_layout.addWidget(target_grp)

        # ── Timing ──
        timing_grp  = QGroupBox("Timing")
        timing_form = QFormLayout(timing_grp)
        timing_form.setSpacing(8)

        self.start_spin = QSpinBox()
        self.start_spin.setRange(0, 999999)
        self.start_spin.setSuffix(" ms")
        self.start_spin.valueChanged.connect(self._recalc_end)
        timing_form.addRow("Start Time:", self.start_spin)

        self.dur_spin = QSpinBox()
        self.dur_spin.setRange(0, 999999)
        self.dur_spin.setSuffix(" ms")
        self.dur_spin.valueChanged.connect(self._recalc_end)
        timing_form.addRow("Duration:", self.dur_spin)

        self.end_lbl = QLabel("0 ms")
        self.end_lbl.setStyleSheet("font-style:italic;")
        timing_form.addRow("End Time (auto):", self.end_lbl)

        self.freq_spin = QSpinBox()
        self.freq_spin.setRange(0, 999999)
        self.freq_spin.setSuffix(" ms")
        self.freq_spin.setToolTip(
            "0 = fire once during the Start→End window.\n"
            "N > 0 = repeat every N ms within the window.\n"
            "Example: Start=100, Duration=500, Freq=100 fires at 100ms, 200ms, 300ms, 400ms, 500ms."
        )
        timing_form.addRow("Frequency:", self.freq_spin)

        freq_hint = QLabel("0 = fire once.  N > 0 = repeat every N ms inside the window.")
        freq_hint.setObjectName("hint_label")
        timing_form.addRow("", freq_hint)

        main_layout.addWidget(timing_grp)
        main_layout.addStretch()

        scroll.setWidget(content)
        outer.addWidget(scroll, 1)

        # Buttons outside scroll so they're always visible
        btn_row = QHBoxLayout()
        btn_row.setContentsMargins(16, 0, 16, 0)
        btn_row.addStretch()
        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        ok_btn = QPushButton("Save Fault")
        ok_btn.setObjectName("btn_primary")
        ok_btn.clicked.connect(self._on_accept)
        btn_row.addWidget(cancel_btn)
        btn_row.addWidget(ok_btn)
        outer.addLayout(btn_row)

    def _update_service_ids(self):
        mid = self.module_cb.currentData()
        self.service_cb.blockSignals(True)
        self.service_cb.clear()
        sids = SERVICE_IDS.get(mid, {})
        if sids:
            for combined, name in sids.items():
                self.service_cb.addItem(f"{name}  (0x{combined:04X})", combined)
        else:
            self.service_cb.addItem("ALL  (0x0000)", 0x0000)
        self.service_cb.blockSignals(False)

    def _update_bit_fields(self):
        ft         = self.type_cb.currentData()
        show_bit   = ft in BITPOS_TYPES
        show_mask  = ft in MASK_TYPES
        show_erase = ft in PARTIAL_ERASE_TYPES
        self.bitpos_row_lbl.setVisible(show_bit)
        self.bitpos_widget.setVisible(show_bit)
        self.mask_row_lbl.setVisible(show_mask)
        self.mask_edit.setVisible(show_mask)
        self.max_erase_row_lbl.setVisible(show_erase)
        self.max_erase_spin.setVisible(show_erase)
        self.max_erase_hint.setVisible(show_erase)

    def _recalc_end(self):
        end = self.start_spin.value() + self.dur_spin.value()
        self.end_lbl.setText(f"{end} ms")

    def _populate(self, f: dict):
        self.fault_id_lbl.setText(str(f.get("FaultID", "?")))
        self.active_cb.setChecked(bool(f.get("Active", 1)))

        mid = f.get("TargetModuleID", 20)
        idx = self.module_cb.findData(mid)
        if idx >= 0:
            self.module_cb.setCurrentIndex(idx)
        self._update_service_ids()

        sid = f.get("TargetModuleServiceID", 0)
        idx = self.service_cb.findData(sid)
        if idx >= 0:
            self.service_cb.setCurrentIndex(idx)

        ft = f.get("Type", 1)
        idx = self.type_cb.findData(ft)
        if idx >= 0:
            self.type_cb.setCurrentIndex(idx)

        # Restore byte + bit from flat BitPosition
        bp = f.get("BitPosition", 0)
        self.byte_spin.setValue(bp // 8)
        self.bit_spin.setValue(bp % 8)

        self.mask_edit.setText(hex(f.get("Mask", 0xFF)))
        self.max_erase_spin.setValue(f.get("MaxEraseBytes", 512))
        self.start_spin.setValue(f.get("Start_TimeMs", 1000))
        self.dur_spin.setValue(f.get("DurationMs", 500))
        self.freq_spin.setValue(f.get("Freq", 0))
        self._recalc_end()

    def _on_accept(self):
        ft = self.type_cb.currentData()
        if ft in MASK_TYPES:
            try:
                int(self.mask_edit.text(), 16)
            except ValueError:
                QMessageBox.warning(self, "Validation Error",
                    "Mask must be a valid hex value (e.g. 0xFF).")
                return
        self.accept()

    def get_fault(self) -> dict:
        ft    = self.type_cb.currentData()
        start = self.start_spin.value()
        dur   = self.dur_spin.value()
        mid   = self.module_cb.currentData()
        sid   = self.service_cb.currentData() if self.service_cb.count() else 0
        f = {
            "TargetModuleID":        mid,
            "TargetModuleServiceID": sid,
            "Type":                  ft,
            "Active":                1 if self.active_cb.isChecked() else 0,
            "Start_TimeMs":          start,
            "DurationMs":            dur,
            "End_timeMs":            start + dur,
            "Freq":                  self.freq_spin.value(),
        }
        if ft in BITPOS_TYPES:
            f["BitPosition"] = self.byte_spin.value() * 8 + self.bit_spin.value()
        elif ft in MASK_TYPES:
            f["Mask"] = int(self.mask_edit.text(), 16)
        elif ft in PARTIAL_ERASE_TYPES:
            f["MaxEraseBytes"] = self.max_erase_spin.value()
        else:
            f["BitPosition"] = 0
        return f


# ─────────────────────────────────────────────────────────────────────────────
#  TEST CASE DIALOG
# ─────────────────────────────────────────────────────────────────────────────

class TestCaseDialog(QDialog):
    def __init__(self, parent=None, tc: dict = None,
                 used_ids: set = None, available_faults: list = None):
        super().__init__(parent)
        self.tc           = tc or {}
        self.used_ids     = used_ids or set()
        self.avail_faults = available_faults or []
        self.setWindowTitle("Edit Test Case" if tc else "Add Test Case")
        self.setMinimumWidth(540)
        self.setMinimumHeight(580)
        self.setModal(True)
        self._build_ui()
        if tc:
            self._populate(tc)

    def _build_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setSpacing(12)
        main_layout.setContentsMargins(16, 16, 16, 16)

        # ── Identity ──
        id_grp  = QGroupBox("Test Case Identity")
        id_form = QFormLayout(id_grp)
        id_form.setSpacing(8)

        self.tc_id_lbl = QLabel("(auto-assigned)")
        self.tc_id_lbl.setStyleSheet("color:#4B6EAF; font-weight:bold;")
        id_form.addRow("Test ID:", self.tc_id_lbl)

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("e.g. BitFlip_Byte10")
        id_form.addRow("Name:", self.name_edit)

        self.desc_edit = QTextEdit()
        self.desc_edit.setFixedHeight(60)
        self.desc_edit.setPlaceholderText("Describe what this test verifies…")
        id_form.addRow("Description:", self.desc_edit)
        main_layout.addWidget(id_grp)

        # ── Config ──
        cfg_grp  = QGroupBox("Test Configuration")
        cfg_form = QFormLayout(cfg_grp)
        cfg_form.setSpacing(8)

        self.timeout_spin = QSpinBox()
        self.timeout_spin.setRange(0, 99999)
        self.timeout_spin.setSuffix(" ms")
        self.timeout_spin.setValue(2000)
        cfg_form.addRow("Timeout:", self.timeout_spin)

        self.result_cb = QComboBox()
        for val, name in EXPECTED_RESULTS.items():
            self.result_cb.addItem(name, val)
        cfg_form.addRow("Expected Result:", self.result_cb)
        main_layout.addWidget(cfg_grp)

        # ── Linked Faults ──
        faults_grp = QGroupBox("Linked Faults (select faults to attach)")
        fl = QVBoxLayout(faults_grp)

        hint = QLabel(f"Max {MAX_FAULTS_PER_TEST} faults per test case.")
        hint.setObjectName("hint_label")
        fl.addWidget(hint)

        self.fault_list = QListWidget()
        self.fault_list.setAlternatingRowColors(True)
        self.fault_list.setMaximumHeight(150)

        for f in self.avail_faults:
            fid   = f.get("FaultID", "?")
            ftype = FAULT_TYPES.get(f.get("Type", 0), "?")
            target = fault_target_label(f)
            label  = f"#{fid}  —  {ftype}  @  {target}"
            item = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, fid)
            item.setCheckState(Qt.CheckState.Unchecked)
            self.fault_list.addItem(item)

        if not self.avail_faults:
            self.fault_list.addItem(QListWidgetItem("  (no faults configured yet)"))

        fl.addWidget(self.fault_list)

        sel_row = QHBoxLayout()
        sel_all = QPushButton("Select All")
        sel_all.clicked.connect(lambda: self._toggle_all(True))
        clr_all = QPushButton("Clear All")
        clr_all.clicked.connect(lambda: self._toggle_all(False))
        sel_row.addWidget(sel_all)
        sel_row.addWidget(clr_all)
        sel_row.addStretch()
        self.sel_count_lbl = QLabel("0 selected")
        self.sel_count_lbl.setObjectName("hint_label")
        sel_row.addWidget(self.sel_count_lbl)
        fl.addLayout(sel_row)

        self.fault_list.itemChanged.connect(self._update_sel_count)
        main_layout.addWidget(faults_grp)

        # ── Buttons ──
        btn_row = QHBoxLayout()
        btn_row.addStretch()
        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        ok_btn = QPushButton("Save Test Case")
        ok_btn.setObjectName("btn_primary")
        ok_btn.clicked.connect(self._on_accept)
        btn_row.addWidget(cancel_btn)
        btn_row.addWidget(ok_btn)
        main_layout.addLayout(btn_row)

    def _toggle_all(self, checked: bool):
        for i in range(self.fault_list.count()):
            item = self.fault_list.item(i)
            if item.data(Qt.ItemDataRole.UserRole) is not None:
                item.setCheckState(Qt.CheckState.Checked if checked else Qt.CheckState.Unchecked)

    def _update_sel_count(self):
        count = sum(
            1 for i in range(self.fault_list.count())
            if self.fault_list.item(i).checkState() == Qt.CheckState.Checked
        )
        self.sel_count_lbl.setText(f"{count} selected")

    def _populate(self, tc: dict):
        self.tc_id_lbl.setText(str(tc.get("TestID", "?")))
        self.name_edit.setText(tc.get("Name", ""))
        self.desc_edit.setPlainText(tc.get("Description", ""))
        self.timeout_spin.setValue(tc.get("TimeoutMs", 2000))
        er  = tc.get("ExpectedResult", 0)
        idx = self.result_cb.findData(er)
        if idx >= 0:
            self.result_cb.setCurrentIndex(idx)
        linked = set(tc.get("FaultIDs", []))
        for i in range(self.fault_list.count()):
            item = self.fault_list.item(i)
            fid  = item.data(Qt.ItemDataRole.UserRole)
            if fid in linked:
                item.setCheckState(Qt.CheckState.Checked)
        self._update_sel_count()

    def _on_accept(self):
        if not self.name_edit.text().strip():
            QMessageBox.warning(self, "Validation", "Name cannot be empty.")
            return
        selected = self._get_selected_fids()
        if len(selected) > MAX_FAULTS_PER_TEST:
            QMessageBox.warning(self, "Validation",
                f"Maximum {MAX_FAULTS_PER_TEST} faults per test case.")
            return
        self.accept()

    def _get_selected_fids(self) -> list:
        result = []
        for i in range(self.fault_list.count()):
            item = self.fault_list.item(i)
            if item.checkState() == Qt.CheckState.Checked:
                fid = item.data(Qt.ItemDataRole.UserRole)
                if fid is not None:
                    result.append(fid)
        return result

    def get_test_case(self) -> dict:
        return {
            "Name":           self.name_edit.text().strip(),
            "Description":    self.desc_edit.toPlainText().strip(),
            "TimeoutMs":      self.timeout_spin.value(),
            "ExpectedResult": self.result_cb.currentData(),
            "FaultIDs":       self._get_selected_fids(),
        }


# ─────────────────────────────────────────────────────────────────────────────
#  FAULT TABLE PANEL
# ─────────────────────────────────────────────────────────────────────────────

class FaultTablePanel(QWidget):
    # Column indices (after combining Module+Service into "Target")
    COL_ID, COL_TARGET, COL_TYPE, COL_START, COL_DUR, COL_END, COL_FREQ, COL_BIT, COL_ACTIVE = range(9)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(6)

        btn_row = QHBoxLayout()
        self.add_btn  = QPushButton("＋  Add Fault")
        self.add_btn.setObjectName("btn_primary")
        self.edit_btn = QPushButton("✎  Edit")
        self.edit_btn.setEnabled(False)
        self.del_btn  = QPushButton("✖  Delete")
        self.del_btn.setObjectName("btn_danger")
        self.del_btn.setEnabled(False)
        lbl = QLabel("Fault Configuration")
        lbl.setObjectName("section_label")
        btn_row.addWidget(lbl)
        btn_row.addStretch()
        btn_row.addWidget(self.add_btn)
        btn_row.addWidget(self.edit_btn)
        btn_row.addWidget(self.del_btn)
        layout.addLayout(btn_row)

        cols = ["ID", "Target", "Type", "Start (ms)", "Duration (ms)",
                "End (ms)", "Freq", "Bit/Mask", "Active"]
        self.table = QTableWidget(0, len(cols))
        self.table.setHorizontalHeaderLabels(cols)
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.horizontalHeader().setSectionResizeMode(self.COL_TARGET, QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(self.COL_TYPE,   QHeaderView.ResizeMode.Stretch)
        self.table.setShowGrid(True)
        self.table.itemSelectionChanged.connect(self._on_selection)
        layout.addWidget(self.table)

    def _on_selection(self):
        has = bool(self.table.selectedItems())
        self.edit_btn.setEnabled(has)
        self.del_btn.setEnabled(has)

    def populate(self, faults: list):
        self.table.setRowCount(0)
        for f in faults:
            r  = self.table.rowCount()
            self.table.insertRow(r)
            ft        = f.get("Type", 0)
            is_active = bool(f.get("Active", 1))

            bval = ""
            if ft in BITPOS_TYPES:
                bp   = f.get("BitPosition", 0)
                bval = f"byte {bp // 8}  bit {bp % 8}"
            elif ft in MASK_TYPES:
                bval = hex(f.get("Mask", 0xFF))
            elif ft in PARTIAL_ERASE_TYPES:
                bval = f"max {f.get('MaxEraseBytes', 512)} B"

            cells = [
                str(f.get("FaultID", "")),
                fault_target_label(f),
                FAULT_TYPES.get(ft, "?"),
                str(f.get("Start_TimeMs", "")),
                str(f.get("DurationMs", "")),
                str(f.get("End_timeMs", "")),
                str(f.get("Freq", 0)),
                bval,
                "✔" if is_active else "✘",
            ]

            # Inactive faults: dim all text
            dim_color = QColor("#666666")

            for c, val in enumerate(cells):
                item = QTableWidgetItem(val)
                item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                if c == self.COL_ACTIVE:
                    item.setForeground(QColor("#4CAF50") if is_active else QColor("#FF5555"))
                elif not is_active:
                    item.setForeground(dim_color)
                self.table.setItem(r, c, item)

        self.table.resizeColumnsToContents()
        self.table.horizontalHeader().setSectionResizeMode(self.COL_TARGET, QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(self.COL_TYPE,   QHeaderView.ResizeMode.Stretch)

    def selected_row(self) -> int:
        rows = self.table.selectedItems()
        return self.table.row(rows[0]) if rows else -1

    def selected_fault_id(self):
        r = self.selected_row()
        if r >= 0:
            item = self.table.item(r, self.COL_ID)
            if item:
                return int(item.text())
        return None


# ─────────────────────────────────────────────────────────────────────────────
#  TEST CASE TABLE PANEL
# ─────────────────────────────────────────────────────────────────────────────

class TestCaseTablePanel(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(6)

        btn_row = QHBoxLayout()
        self.add_btn  = QPushButton("＋  Add Test Case")
        self.add_btn.setObjectName("btn_primary")
        self.edit_btn = QPushButton("✎  Edit")
        self.edit_btn.setEnabled(False)
        self.del_btn  = QPushButton("✖  Delete")
        self.del_btn.setObjectName("btn_danger")
        self.del_btn.setEnabled(False)
        lbl = QLabel("Test Cases")
        lbl.setObjectName("section_label")
        btn_row.addWidget(lbl)
        btn_row.addStretch()
        btn_row.addWidget(self.add_btn)
        btn_row.addWidget(self.edit_btn)
        btn_row.addWidget(self.del_btn)
        layout.addLayout(btn_row)

        cols = ["ID", "Name", "Timeout (ms)", "Expected Result", "Linked Fault IDs"]
        self.table = QTableWidget(0, len(cols))
        self.table.setHorizontalHeaderLabels(cols)
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)
        self.table.setShowGrid(True)
        self.table.itemSelectionChanged.connect(self._on_selection)
        layout.addWidget(self.table)

    def _on_selection(self):
        has = bool(self.table.selectedItems())
        self.edit_btn.setEnabled(has)
        self.del_btn.setEnabled(has)

    def populate(self, test_cases: list):
        self.table.setRowCount(0)
        for tc in test_cases:
            r    = self.table.rowCount()
            self.table.insertRow(r)
            fids = tc.get("FaultIDs", [])
            cells = [
                str(tc.get("TestID", "")),
                tc.get("Name", ""),
                str(tc.get("TimeoutMs", "")),
                EXPECTED_RESULTS.get(tc.get("ExpectedResult", 0), "?"),
                ", ".join(str(x) for x in fids) if fids else "—",
            ]
            for c, val in enumerate(cells):
                item = QTableWidgetItem(val)
                if c in (0, 2):
                    item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                if c == 3:
                    color = {"DATA_CORRUPTED": "#FF5555",
                             "DATA_CLEAN":     "#4CAF50",
                             "MANUAL":         "#E6B800"}.get(val, "#BBBBBB")
                    item.setForeground(QColor(color))
                self.table.setItem(r, c, item)
        self.table.resizeColumnsToContents()
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)

    def selected_row(self) -> int:
        rows = self.table.selectedItems()
        return self.table.row(rows[0]) if rows else -1

    def selected_tc_id(self):
        r = self.selected_row()
        if r >= 0:
            item = self.table.item(r, 0)
            if item:
                return int(item.text())
        return None


# ─────────────────────────────────────────────────────────────────────────────
#  MAIN WINDOW
# ─────────────────────────────────────────────────────────────────────────────

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.settings   = load_settings()
        self.faults     : list = []
        self.test_cases : list = []
        self._dirty = False

        self.setWindowTitle("Fault Injection Framework")
        self.resize(1440, 860)

        self._build_menu()
        self._build_toolbar()
        self._build_central()
        self._build_docks()
        self._build_statusbar()
        self._apply_theme(self.settings.get("theme", "dark"))
        self._load_data()
        self._refresh_all()

    # ── Dirty state ───────────────────────────────────────────────────────────
    def _set_dirty(self, dirty: bool = True):
        self._dirty = dirty
        base = "Fault Injection Framework"
        self.setWindowTitle(f"{base} *" if dirty else base)

    # ── Theme ─────────────────────────────────────────────────────────────────
    def _apply_theme(self, theme: str):
        self.settings["theme"] = theme
        QApplication.instance().setStyleSheet(
            DARK_STYLE if theme == "dark" else LIGHT_STYLE
        )
        self.theme_action.setText("Switch to Light Mode" if theme == "dark"
                                   else "Switch to Dark Mode")

    def _toggle_theme(self):
        new = "light" if self.settings.get("theme") == "dark" else "dark"
        self._apply_theme(new)
        save_settings(self.settings)

    # ── Menu ──────────────────────────────────────────────────────────────────
    def _build_menu(self):
        mb = self.menuBar()

        file_menu = mb.addMenu("File")
        file_menu.addAction("New Configuration").triggered.connect(self._on_new)
        file_menu.addAction("Open Folder…").triggered.connect(self._on_open_folder)
        file_menu.addSeparator()
        save_a = file_menu.addAction("Save")
        save_a.setShortcut(QKeySequence("Ctrl+S"))
        save_a.triggered.connect(self._on_save)
        file_menu.addSeparator()
        file_menu.addAction("Exit").triggered.connect(self.close)

        edit_menu = mb.addMenu("Edit")
        edit_menu.addAction("Add Fault").triggered.connect(self._add_fault)
        edit_menu.addAction("Delete Fault").triggered.connect(self._del_fault)
        edit_menu.addSeparator()
        edit_menu.addAction("Add Test Case").triggered.connect(self._add_tc)
        edit_menu.addAction("Delete Test Case").triggered.connect(self._del_tc)

        view_menu = mb.addMenu("View")
        self.theme_action = view_menu.addAction("Switch to Light Mode")
        self.theme_action.triggered.connect(self._toggle_theme)

        help_menu = mb.addMenu("Help")
        help_menu.addAction("About").triggered.connect(lambda: QMessageBox.information(
            self, "About",
            "Fault Injection Studio\nPyQt6 GUI for AUTOSAR Fault Injection Framework\n"
            "Graduation Project — Embedded Memory Stack Validation\n\n"
            "Reads/writes fault_config.json & test_cases.json"))

    # ── Toolbar ───────────────────────────────────────────────────────────────
    def _build_toolbar(self):
        tb = QToolBar("Main Toolbar")
        tb.setIconSize(QSize(16, 16))
        tb.setMovable(False)
        self.addToolBar(tb)

        tb.addAction("Save").triggered.connect(self._on_save)
        tb.addSeparator()
        tb.addAction("＋ Add Fault").triggered.connect(self._add_fault)
        tb.addAction("✖ Del Fault").triggered.connect(self._del_fault)
        tb.addSeparator()
        tb.addAction("＋ Add Test Case").triggered.connect(self._add_tc)
        tb.addAction("✖ Del Test Case").triggered.connect(self._del_tc)
        tb.addSeparator()
        tb.addAction("Theme").triggered.connect(self._toggle_theme)

    # ── Central widget ────────────────────────────────────────────────────────
    def _build_central(self):
        self.center_tabs = QTabWidget()
        self.center_tabs.setTabPosition(QTabWidget.TabPosition.North)

        self.fault_panel = FaultTablePanel()
        self.fault_panel.add_btn.clicked.connect(self._add_fault)
        self.fault_panel.edit_btn.clicked.connect(self._edit_fault)
        self.fault_panel.del_btn.clicked.connect(self._del_fault)
        self.fault_panel.table.itemSelectionChanged.connect(self._on_fault_selected)

        self.tc_panel = TestCaseTablePanel()
        self.tc_panel.add_btn.clicked.connect(self._add_tc)
        self.tc_panel.edit_btn.clicked.connect(self._edit_tc)
        self.tc_panel.del_btn.clicked.connect(self._del_tc)
        self.tc_panel.table.itemSelectionChanged.connect(self._on_tc_selected)

        self.center_tabs.addTab(self.fault_panel, "Fault Configuration")
        self.center_tabs.addTab(self.tc_panel,    "Test Cases")
        self.setCentralWidget(self.center_tabs)

    # ── Dock widgets ──────────────────────────────────────────────────────────
    def _build_docks(self):
        L = Qt.DockWidgetArea.LeftDockWidgetArea
        R = Qt.DockWidgetArea.RightDockWidgetArea
        B = Qt.DockWidgetArea.BottomDockWidgetArea

        # Project Explorer
        self.proj_tree = QTreeWidget()
        self.proj_tree.setHeaderLabel("Project Explorer")
        self._populate_project_tree()
        proj_dock = self._add_dock("Project Explorer", self.proj_tree, L)

        # Outline
        self.outline_tree = QTreeWidget()
        self.outline_tree.setHeaderLabel("Outline")
        self.outline_tree.setColumnCount(2)
        self.outline_tree.setHeaderLabels(["Field", "Value"])
        outline_dock = self._add_dock("Outline", self.outline_tree, L)
        self.splitDockWidget(proj_dock, outline_dock, Qt.Orientation.Vertical)

        # Fault Summary (replaces empty Workflows sidebar)
        self._summary_widget = QWidget()
        sl = QVBoxLayout(self._summary_widget)
        sl.setContentsMargins(8, 8, 8, 8)
        sl.setSpacing(4)
        sum_lbl = QLabel("Fault Summary")
        sum_lbl.setObjectName("section_label")
        sl.addWidget(sum_lbl)
        self._summary_text = QTextEdit()
        self._summary_text.setReadOnly(True)
        self._summary_text.setFrameShape(QFrame.Shape.NoFrame)
        self._summary_text.setFont(QFont("Segoe UI", 11))
        sl.addWidget(self._summary_text)
        summary_dock = self._add_dock("Fault Summary", self._summary_widget, R)
        summary_dock.setMinimumWidth(190)
        summary_dock.setMaximumWidth(260)

        # Error Log (bottom)
        self.error_table = QTableWidget(0, 4)
        self.error_table.setHorizontalHeaderLabels(["", "Message", "Source", "Timestamp"])
        self.error_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.error_table.verticalHeader().setVisible(False)
        self.error_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.error_table.setShowGrid(True)
        self.error_table.setAlternatingRowColors(True)
        self.error_table.setMinimumHeight(100)
        err_dock = self._add_dock("Error Log  /  Problems", self.error_table, B)

        # JSON Preview (bottom, alongside Error Log)
        self._json_preview = QTextEdit()
        self._json_preview.setReadOnly(True)
        self._json_preview.setFont(QFont("Courier New", 10))
        self._json_preview.setPlaceholderText("Select a fault to preview its JSON…")
        self._json_preview.setMinimumHeight(100)
        json_dock = self._add_dock("JSON Preview", self._json_preview, B)
        self.splitDockWidget(err_dock, json_dock, Qt.Orientation.Horizontal)

        # Properties (bottom)
        self.props_table = QTableWidget(0, 2)
        self.props_table.setHorizontalHeaderLabels(["Property", "Value"])
        self.props_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.props_table.verticalHeader().setVisible(False)
        self.props_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.props_table.setShowGrid(True)
        self.props_table.setMinimumHeight(100)
        props_dock = self._add_dock("Properties", self.props_table, B)
        self.splitDockWidget(json_dock, props_dock, Qt.Orientation.Horizontal)

    def _add_dock(self, title, widget, area):
        dock = QDockWidget(title, self)
        dock.setWidget(widget)
        dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetMovable |
            QDockWidget.DockWidgetFeature.DockWidgetFloatable
        )
        self.addDockWidget(area, dock)
        return dock

    def _populate_project_tree(self):
        self.proj_tree.clear()
        root = QTreeWidgetItem(self.proj_tree, ["MemStack_Testing"])
        ecu  = QTreeWidgetItem(root, ["Fault_Injection_ECU"])
        QTreeWidgetItem(ecu, [f"fault_config.json  ({len(self.faults)} faults)"])
        QTreeWidgetItem(ecu, [f"test_cases.json  ({len(self.test_cases)} tests)"])
        QTreeWidgetItem(root, ["src"])
        QTreeWidgetItem(root, ["Include"])
        root.setExpanded(True)
        ecu.setExpanded(True)

    def _update_summary(self):
        total    = len(self.faults)
        active   = sum(1 for f in self.faults if f.get("Active", 1))
        inactive = total - active

        by_type = {}
        for f in self.faults:
            name = FAULT_TYPES.get(f.get("Type", 0), "?")
            by_type[name] = by_type.get(name, 0) + 1

        by_module = {}
        for f in self.faults:
            name = module_short(f.get("TargetModuleID", 0))
            by_module[name] = by_module.get(name, 0) + 1

        lines = [
            f"Faults:   {total} / {MAX_FAULTS}",
            f"Active:   {active}   Inactive: {inactive}",
            "",
            "── By Type ──────────",
        ]
        for name, count in sorted(by_type.items()):
            lines.append(f"  {name}: {count}")
        lines += ["", "── By Module ─────────"]
        for name, count in sorted(by_module.items()):
            lines.append(f"  {name}: {count}")
        lines += ["", f"Test Cases: {len(self.test_cases)} / {MAX_TEST_CASES}"]
        self._summary_text.setPlainText("\n".join(lines))

    # ── Status bar ────────────────────────────────────────────────────────────
    def _build_statusbar(self):
        self.sb       = self.statusBar()
        self.sb_main  = QLabel()
        self.sb_path  = QLabel()
        self.sb_count = QLabel()
        self.sb.addWidget(self.sb_main)
        self.sb.addPermanentWidget(self.sb_count)
        self.sb.addPermanentWidget(self.sb_path)
        self._update_statusbar("Ready")

    def _update_statusbar(self, msg: str = ""):
        self.sb_main.setText(f"  {msg}")
        self.sb_path.setText(f"  {SCRIPT_DIR}  ")
        self.sb_count.setText(
            f"  Faults: {len(self.faults)}/{MAX_FAULTS}   "
            f"Tests: {len(self.test_cases)}/{MAX_TEST_CASES}  "
        )

    # ── Data I/O ──────────────────────────────────────────────────────────────
    def _load_data(self):
        self.faults     = load_json(FAULT_JSON, [])
        self.test_cases = load_json(TEST_JSON,  [])
        self._set_dirty(False)
        self._validate_loaded()

    def _validate_loaded(self):
        for f in self.faults:
            start = f.get("Start_TimeMs", 0)
            end   = f.get("End_timeMs", 0)
            if end != 0 and start >= end:
                self._log(
                    f"Fault #{f.get('FaultID','?')}: Start_TimeMs ({start}) >= End_timeMs ({end}) — timing is invalid",
                    "WARNING"
                )

    def _on_save(self):
        ok1 = save_json(FAULT_JSON,    self.faults)
        ok2 = save_json(TEST_JSON,     self.test_cases)
        save_settings(self.settings)
        if ok1 and ok2:
            self._set_dirty(False)
            self._update_statusbar("✔  Saved successfully")
            self._log("Saved fault_config.json and test_cases.json", "INFO")
            QTimer.singleShot(3000, lambda: self._update_statusbar("Ready"))
        else:
            self._update_statusbar("✖  Save failed!")
            self._log("Save failed — check file permissions", "ERROR")

    def _on_new(self):
        if self._dirty:
            r = QMessageBox.question(self, "Unsaved Changes",
                "Discard unsaved changes and start fresh?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if r != QMessageBox.StandardButton.Yes:
                return
        self.faults     = []
        self.test_cases = []
        self._set_dirty(True)
        self._refresh_all()
        self._log("New configuration started", "INFO")

    def _on_open_folder(self):
        global FAULT_JSON, TEST_JSON, SCRIPT_DIR
        from PyQt6.QtWidgets import QFileDialog
        folder = QFileDialog.getExistingDirectory(self, "Select project folder", SCRIPT_DIR)
        if folder:
            FAULT_JSON  = os.path.join(folder, "fault_config.json")
            TEST_JSON   = os.path.join(folder, "test_cases.json")
            SCRIPT_DIR  = folder
            self._load_data()
            self._refresh_all()
            self._update_statusbar(f"Opened: {folder}")
            self._log(f"Opened folder: {folder}", "INFO")

    # ── Refresh ───────────────────────────────────────────────────────────────
    def _refresh_all(self):
        self.fault_panel.populate(self.faults)
        self.tc_panel.populate(self.test_cases)
        self._populate_project_tree()
        self._update_summary()
        self._update_statusbar()
        self._validate_all()

    def _validate_all(self):
        fault_ids = {f["FaultID"] for f in self.faults}
        for tc in self.test_cases:
            for fid in tc.get("FaultIDs", []):
                if fid not in fault_ids:
                    self._log(
                        f"Test '{tc.get('Name','?')}' references missing FaultID {fid}",
                        "WARNING"
                    )

    # ── CRUD — Faults ─────────────────────────────────────────────────────────
    def _add_fault(self):
        if len(self.faults) >= MAX_FAULTS:
            QMessageBox.warning(self, "Limit Reached",
                f"Maximum of {MAX_FAULTS} faults already configured.")
            return
        used = {f["FaultID"] for f in self.faults}
        dlg  = FaultDialog(self, used_ids=used)
        if dlg.exec() == QDialog.DialogCode.Accepted:
            new_id = next_free_id(used, MAX_FAULTS)
            data   = dlg.get_fault()
            # Duplicate check: same ServiceID + same Type
            for f in self.faults:
                if (f.get("TargetModuleServiceID") == data.get("TargetModuleServiceID") and
                        f.get("Type") == data.get("Type")):
                    r = QMessageBox.question(self, "Duplicate Fault",
                        f"A fault with the same service target and type already exists.\nAdd anyway?",
                        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
                    if r != QMessageBox.StandardButton.Yes:
                        return
                    break
            data["FaultID"] = new_id
            self.faults.append(data)
            self._set_dirty()
            self._refresh_all()
            self._log(f"Added Fault #{new_id}", "INFO")

    def _edit_fault(self):
        fid = self.fault_panel.selected_fault_id()
        if fid is None:
            return
        fault = next((f for f in self.faults if f["FaultID"] == fid), None)
        if not fault:
            return
        used = {f["FaultID"] for f in self.faults if f["FaultID"] != fid}
        dlg  = FaultDialog(self, fault=fault, used_ids=used)
        if dlg.exec() == QDialog.DialogCode.Accepted:
            data = dlg.get_fault()
            data["FaultID"] = fid
            idx = next(i for i, f in enumerate(self.faults) if f["FaultID"] == fid)
            self.faults[idx] = data
            self._set_dirty()
            self._refresh_all()
            self._log(f"Updated Fault #{fid}", "INFO")

    def _del_fault(self):
        fid = self.fault_panel.selected_fault_id()
        if fid is None:
            return
        linked_tests = [tc["Name"] for tc in self.test_cases if fid in tc.get("FaultIDs", [])]
        msg = f"Delete Fault #{fid}?"
        if linked_tests:
            msg += (f"\n\n⚠ Linked to {len(linked_tests)} test case(s):\n"
                    + "\n".join(f"  • {n}" for n in linked_tests)
                    + "\n\nThe fault will be unlinked from those tests.")
        r = QMessageBox.question(self, "Delete Fault", msg,
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if r != QMessageBox.StandardButton.Yes:
            return
        self.faults = [f for f in self.faults if f["FaultID"] != fid]
        for tc in self.test_cases:
            tc["FaultIDs"] = [x for x in tc.get("FaultIDs", []) if x != fid]
        self._set_dirty()
        self._refresh_all()
        self._log(f"Deleted Fault #{fid}", "INFO")

    # ── CRUD — Test Cases ─────────────────────────────────────────────────────
    def _add_tc(self):
        if len(self.test_cases) >= MAX_TEST_CASES:
            QMessageBox.warning(self, "Limit Reached",
                f"Maximum of {MAX_TEST_CASES} test cases already configured.")
            return
        used  = {tc["TestID"] for tc in self.test_cases}
        avail = sorted(self.faults, key=lambda f: f["FaultID"])
        dlg   = TestCaseDialog(self, available_faults=avail)
        if dlg.exec() == QDialog.DialogCode.Accepted:
            new_id = next_free_id(used, MAX_TEST_CASES)
            data   = dlg.get_test_case()
            data["TestID"] = new_id
            self.test_cases.append(data)
            self._set_dirty()
            self._refresh_all()
            self._log(f"Added Test Case #{new_id}: {data['Name']}", "INFO")

    def _edit_tc(self):
        tc_id = self.tc_panel.selected_tc_id()
        if tc_id is None:
            return
        tc = next((t for t in self.test_cases if t["TestID"] == tc_id), None)
        if not tc:
            return
        used  = {t["TestID"] for t in self.test_cases if t["TestID"] != tc_id}
        avail = sorted(self.faults, key=lambda f: f["FaultID"])
        dlg   = TestCaseDialog(self, tc=tc, used_ids=used, available_faults=avail)
        if dlg.exec() == QDialog.DialogCode.Accepted:
            data = dlg.get_test_case()
            data["TestID"] = tc_id
            idx  = next(i for i, t in enumerate(self.test_cases) if t["TestID"] == tc_id)
            self.test_cases[idx] = data
            self._set_dirty()
            self._refresh_all()
            self._log(f"Updated Test Case #{tc_id}", "INFO")

    def _del_tc(self):
        tc_id = self.tc_panel.selected_tc_id()
        if tc_id is None:
            return
        tc = next((t for t in self.test_cases if t["TestID"] == tc_id), None)
        r  = QMessageBox.question(self, "Delete Test Case",
            f"Delete Test Case #{tc_id} '{tc.get('Name', '')}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if r != QMessageBox.StandardButton.Yes:
            return
        self.test_cases = [t for t in self.test_cases if t["TestID"] != tc_id]
        self._set_dirty()
        self._refresh_all()
        self._log(f"Deleted Test Case #{tc_id}", "INFO")

    # ── Selection handlers ────────────────────────────────────────────────────
    def _on_fault_selected(self):
        fid = self.fault_panel.selected_fault_id()
        if fid is None:
            return
        fault = next((f for f in self.faults if f["FaultID"] == fid), None)
        if not fault:
            return

        bp = fault.get("BitPosition", 0)
        self._show_properties({
            "FaultID":               str(fid),
            "Target":                fault_target_label(fault),
            "Type":                  FAULT_TYPES.get(fault.get("Type"), "?"),
            "Active":                "Yes" if fault.get("Active") else "No",
            "Start_TimeMs":          str(fault.get("Start_TimeMs", "")),
            "DurationMs":            str(fault.get("DurationMs", "")),
            "End_timeMs":            str(fault.get("End_timeMs", "")),
            "Freq":                  str(fault.get("Freq", 0)),
            "BitPosition":           f"byte {bp // 8}  bit {bp % 8}" if "BitPosition" in fault else "—",
            "Mask":                  hex(fault.get("Mask", 0)) if "Mask" in fault else "—",
            "MaxEraseBytes":         str(fault.get("MaxEraseBytes", "—")),
        })
        self._show_outline(fault, "Fault")
        self._json_preview.setPlainText(json.dumps(fault, indent=2))

    def _on_tc_selected(self):
        tc_id = self.tc_panel.selected_tc_id()
        if tc_id is None:
            return
        tc   = next((t for t in self.test_cases if t["TestID"] == tc_id), None)
        if not tc:
            return
        fids = tc.get("FaultIDs", [])
        self._show_properties({
            "TestID":         str(tc_id),
            "Name":           tc.get("Name", ""),
            "Description":    tc.get("Description", ""),
            "TimeoutMs":      str(tc.get("TimeoutMs", "")),
            "ExpectedResult": EXPECTED_RESULTS.get(tc.get("ExpectedResult"), "?"),
            "LinkedFaults":   ", ".join(str(x) for x in fids) if fids else "none",
            "FaultCount":     str(len(fids)),
        })
        self._show_outline(tc, "TestCase")
        self._json_preview.setPlainText(json.dumps(tc, indent=2))

    def _show_properties(self, props: dict):
        self.props_table.setRowCount(0)
        for key, val in props.items():
            r = self.props_table.rowCount()
            self.props_table.insertRow(r)
            k_item = QTableWidgetItem(key)
            k_item.setForeground(QColor("#4B6EAF"))
            k_item.setFont(QFont("Segoe UI", 11, QFont.Weight.Bold))
            self.props_table.setItem(r, 0, k_item)
            self.props_table.setItem(r, 1, QTableWidgetItem(val))
        self.props_table.resizeColumnToContents(0)

    def _show_outline(self, obj: dict, kind: str):
        self.outline_tree.clear()
        root = QTreeWidgetItem(self.outline_tree, [kind, ""])
        for k, v in obj.items():
            child = QTreeWidgetItem(root, [str(k), str(v)])
            child.setForeground(0, QColor("#4B6EAF"))
        root.setExpanded(True)
        self.outline_tree.resizeColumnToContents(0)

    # ── Error Log ──────────────────────────────────────────────────────────────
    def _log(self, message: str, level: str = "INFO"):
        icon  = {"INFO": "ℹ", "WARNING": "⚠", "ERROR": "✖"}.get(level, "ℹ")
        color = {"INFO": "#BBBBBB", "WARNING": "#E6B800", "ERROR": "#FF5555"}.get(level, "#BBBBBB")
        ts    = datetime.now().strftime("%H:%M:%S")
        r     = self.error_table.rowCount()
        self.error_table.insertRow(r)
        for c, val in enumerate([icon, message, "GUI", ts]):
            item = QTableWidgetItem(val)
            item.setForeground(QColor(color))
            if c == 0:
                item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.error_table.setItem(r, c, item)
        self.error_table.scrollToBottom()

    # ── Close event ───────────────────────────────────────────────────────────
    def closeEvent(self, event):
        if self._dirty:
            r = QMessageBox.question(self, "Unsaved Changes",
                "You have unsaved changes. Save before closing?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel)
            if r == QMessageBox.StandardButton.Save:
                self._on_save()
            elif r == QMessageBox.StandardButton.Cancel:
                event.ignore()
                return
        save_settings(self.settings)
        event.accept()


# ─────────────────────────────────────────────────────────────────────────────
#  ENTRY POINT
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
