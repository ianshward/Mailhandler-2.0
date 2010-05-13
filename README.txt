Mailhandler 2.0 - Documentation coming soon.

New features and architecture:

* Uses Feeds API and implements a fetcher, parser, and processor
* Uses ctools to:
  * store mailbox definitions in code
  * command parsing and processing plugins
  * authentication plugins

## Todos

This is in an alpha state with several todos noted in the code.  Larger todos include:

* Finish the default commands plugin
* Improve error handling
* Get working with Mail Comment
* Compatibility with other dependent modules

## Quick start

* Create a mailbox on admin/content/mailhandler
* Go to admin/build/feeds and create a new importer using mailhandler fetcher, parser, processor
* Configure settings for parser and mapping + settings for processor
