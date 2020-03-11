# QtPhrasebookUtilityTool
A tool to modify Qt-translation or phrasebook files.

When you have multiple applications and for each one you have translation files, you most likely want to import/use some of your already translated phrases in a new project or simply in one of your other projects. To do this, Qt expects you to use „Phrasebooks“ that can be loaded via Linguist and can then there be used.
Sadly you have to add each translation you want into the Phrasebook by hand and you can link against phrasebooks when you update your translation file via "lupdate".

Linguist simply does not offer a decent export, update or patch feature

This is where this "Phrasebook Utility Tool" comes into play.

With this tool you'll be able to:
- merge two *.ts files into one
- Patch a *.ts file with a phrasebook
- Export *.ts file(s) as phrasebook(s)
- Export multiple *.ts files into a single phrasebook
- Update an existing phrasebook with with a *.ts file or an other phrasebook


Ui information:

File:
The file menu allows you to add *.ts or *.qph files to either the sources or targets list view.
You can also remove selected entries from either view.

From the source and target view, you select your desired sources/targets and via the action menu you decide what function will operate on those selected files.

Actions:
Merge Into Target:
 - Accepts as source only *.ts files
 - Results in a *.ts file

Patch Ts File:
- Accepts as source only *.qph files
- Target file needs to be a *.ts file 
- In the targeted *.ts file, untranslated entries with a matching source string will be patch, if a translation was found  inside the phrasebook. The change needs to be confirmed, later on, via the Linguist tool.

Export as Phrasebooks:
- Turns all selected *.ts files into *.qph files
- Accepts only *.ts files
- Creates new *.qph files named after the source files (qt_de.ts -> qt_de.qph) located parallel to the source file

Export to Target:
- Turns a selected *.ts file into a *.qph file of your liking 
- Accepts only *.ts files
- Results in a single new *.qph file

Update Phrasebook:
- Accepts either *.ts files or *.qph files, but not mixed
- Results in an patched/updated *.qph file
