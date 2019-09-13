#!/bin/sh

# Setup symlinks to WebInspector/UserInterface directories so Worker resources load.
ln -s ../../UserInterface/Workers Workers
ln -s ../../UserInterface/External External
