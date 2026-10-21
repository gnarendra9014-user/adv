
import express from "express";
import mongoose from "mongoose";
import { nanoid } from "nanoid";
import dotenv from "dotenv";
import helmet from "helmet";
import rateLimit from "express-rate-limit";

dotenv.config();

const app = express();
const PORT = process.env.PORT || 3000;

// --------------------
// Middleware
// --------------------

app.use(helmet());
app.use(express.json({ limit: "10kb" }));

// Rate limit URL creation requests
const shortenLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 20,
  standardHeaders: "draft-8",
  legacyHeaders: false,
  message: {
    error: "Too many URL creation requests. Try again later."
  }
});

// --------------------
// MongoDB connection
// --------------------

mongoose
  .connect(process.env.MONGO_URI)
  .then(() => {
    console.log("MongoDB connected successfully");
  })
  .catch((error) => {
    console.error("MongoDB connection failed:", error.message);
    process.exit(1);
  });

// --------------------
// Database schema
// --------------------

const urlSchema = new mongoose.Schema(
  {
    originalUrl: {
      type: String,
      required: true,
      trim: true,
      maxlength: 2048
    },
    shortCode: {
      type: String,
      required: true,
      unique: true,
      index: true
    },
    clicks: {
      type: Number,
      default: 0,
      min: 0
    }
  },
  {
    timestamps: true
  }
);

const URL = mongoose.model("URL", urlSchema);

// --------------------
// Health check
// --------------------

app.get("/health", (req, res) => {
  res.status(200).json({
    status: "OK",
    message: "URL Shortener API is running",
    timestamp: new Date().toISOString()
  });
});

// --------------------
// Create short URL
// --------------------

app.post("/api/shorten", shortenLimiter, async (req, res, next) => {
  try {
    const { originalUrl } = req.body ?? {};

    if (typeof originalUrl !== "string" || !originalUrl.trim()) {
      return res.status(400).json({
        error: "A valid originalUrl is required"
      });
    }

    const cleanedUrl = originalUrl.trim();

    if (cleanedUrl.length > 2048) {
      return res.status(400).json({
        error: "URL must not exceed 2048 characters"
      });
    }

    let parsedUrl;

    try {
      parsedUrl = new URL(cleanedUrl);
    } catch {
      return res.status(400).json({
        error: "Invalid URL format"
      });
    }

    if (!["http:", "https:"].includes(parsedUrl.protocol)) {
      return res.status(400).json({
        error: "Only HTTP and HTTPS URLs are allowed"
      });
    }

    // Retry if a randomly generated code already exists.
    for (let attempt = 0; attempt < 3; attempt++) {
      const shortCode = nanoid(7);

      try {
        const savedUrl = await URL.create({
          originalUrl: cleanedUrl,
          shortCode
        });

        return res.status(201).json({
          message: "Short URL created successfully",
          originalUrl: savedUrl.originalUrl,
          shortCode: savedUrl.shortCode,
          shortUrl: `${process.env.BASE_URL}/${savedUrl.shortCode}`,
          createdAt: savedUrl.createdAt
        });
      } catch (error) {
        // Retry only if MongoDB reports a duplicate key.
        if (error.code === 11000 && attempt < 2) {
          continue;
        }

        throw error;
      }
    }

    return res.status(500).json({
      error: "Could not generate a unique short code"
    });
  } catch (error) {
    next(error);
  }
});

// --------------------
// Redirect to original URL
// --------------------

app.get("/:shortCode", async (req, res, next) => {
  try {
    const { shortCode } = req.params;

    const url = await URL.findOneAndUpdate(
      { shortCode },
      { $inc: { clicks: 1 } },
      { new: true }
    );

    if (!url) {
      return res.status(404).json({
        error: "Short URL not found"
      });
    }

    return res.redirect(302, url.originalUrl);
  } catch (error) {
    next(error);
  }
});

// --------------------
// URL analytics
// --------------------

app.get("/api/stats/:shortCode", async (req, res, next) => {
  try {
    const url = await URL.findOne({
      shortCode: req.params.shortCode
    }).select("originalUrl shortCode clicks createdAt updatedAt");

    if (!url) {
      return res.status(404).json({
        error: "Short URL not found"
      });
    }

    return res.status(200).json({
      originalUrl: url.originalUrl,
      shortCode: url.shortCode,
      shortUrl: `${process.env.BASE_URL}/${url.shortCode}`,
      clicks: url.clicks,
      createdAt: url.createdAt,
      updatedAt: url.updatedAt
    });
  } catch (error) {
    next(error);
  }
});

// --------------------
// Handle unknown API routes
// --------------------

app.use("/api", (req, res) => {
  res.status(404).json({
    error: "API endpoint not found"
  });
});

// --------------------
// Global error handler
// --------------------

app.use((error, req, res, next) => {
  console.error(error);

  if (res.headersSent) {
    return next(error);
  }

  if (error.type === "entity.parse.failed") {
    return res.status(400).json({
      error: "Invalid JSON request body"
    });
  }

  if (error.type === "entity.too.large") {
    return res.status(413).json({
      error: "Request body is too large"
    });
  }

  return res.status(500).json({
    error: "Internal server error"
  });
});

// --------------------
// Start server
// --------------------

async function startServer() {
  try {
    await mongoose.connection.asPromise();

    app.listen(PORT, () => {
      console.log(`Server running at ${process.env.BASE_URL}`);
      console.log(`Health check: ${process.env.BASE_URL}/health`);
    });
  } catch (error) {
    console.error("Server startup failed:", error.message);
    process.exit(1);
  }
}

startServer();
